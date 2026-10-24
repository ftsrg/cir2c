// Copyright 2025 Budapest University of Technology and Economics
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// cir-merge: merge the CIR modules of several translation units into one.
//
// A conservative merger (phases 1 and 2 of the multi-TU design). Every input is parsed
// and verified with MLIR's parser, the module attributes are checked for
// compatibility, a symbol inventory is built, a merge plan is computed for the
// whole inventory, and only then is the output module built, verified,
// re-parsed and written. Anything outside the supported subset is rejected
// with a diagnostic; nothing is dropped silently. The supported subset is
// documented in README.md ("Merging translation units").

#include <clang/CIR/Dialect/IR/CIRAttrs.h>
#include <clang/CIR/Dialect/IR/CIRDialect.h>

#include <mlir/Dialect/DLTI/DLTI.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/AttrTypeSubElements.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/DialectRegistry.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/IR/Verifier.h>
#include <mlir/Parser/Parser.h>
#include <mlir/Support/FileUtilities.h>

#include <llvm/ADT/MapVector.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringSet.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <functional>
#include <memory>
#include <set>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace mlir;

namespace {

llvm::cl::OptionCategory mergeCategory("cir-merge options");
llvm::cl::list<std::string> inputPaths(llvm::cl::Positional, llvm::cl::OneOrMore,
                                       llvm::cl::desc("<input.cir>..."),
                                       llvm::cl::cat(mergeCategory));
llvm::cl::opt<std::string> outputPath("output", llvm::cl::Required,
                                      llvm::cl::desc("Output merged CIR file"),
                                      llvm::cl::value_desc("file"),
                                      llvm::cl::cat(mergeCategory));
llvm::cl::alias outputAlias("o", llvm::cl::aliasopt(outputPath),
                            llvm::cl::desc("Alias for --output"),
                            llvm::cl::cat(mergeCategory));
llvm::cl::opt<bool> dumpSymbols("dump-symbols",
                                llvm::cl::desc("Print the symbol inventory to stdout"),
                                llvm::cl::cat(mergeCategory));
llvm::cl::opt<bool> verbose("verbose",
                            llvm::cl::desc("Explain merge decisions on stderr"),
                            llvm::cl::cat(mergeCategory));

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

std::string locString(Location loc) {
  std::string s;
  llvm::raw_string_ostream os(s);
  loc.print(os);
  return s;
}

llvm::raw_ostream &error() { return llvm::errs() << "cir-merge: error: "; }
llvm::raw_ostream &note() { return llvm::errs() << "cir-merge: note: "; }

// ---------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------

void registerDialects(MLIRContext &context) {
  DialectRegistry registry;
  registry.insert<cir::CIRDialect, DLTIDialect, LLVM::LLVMDialect>();
  context.appendDialectRegistry(registry);
  context.loadAllAvailableDialects();
}

struct Input {
  std::string path;
  OwningOpRef<ModuleOp> module;
};

// Parses and verifies one input into `context`. MLIR's own diagnostics (with
// the file, line and column of the offending text) are printed by the
// SourceMgr handler; this only adds which input was being read.
OwningOpRef<ModuleOp> parseBuffer(std::unique_ptr<llvm::MemoryBuffer> file,
                                  const std::string &path,
                                  MLIRContext &context) {
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(std::move(file), llvm::SMLoc());
  SourceMgrDiagnosticHandler handler(sourceMgr, &context);
  // verifyAfterParse runs the op and dialect verifiers on the parsed module.
  ParserConfig config(&context, /*verifyAfterParse=*/true);
  OwningOpRef<ModuleOp> module = parseSourceFile<ModuleOp>(sourceMgr, config);
  if (!module) {
    error() << path << ": not a valid CIR module (see the parser or verifier "
            << "error above)\n";
    return nullptr;
  }
  return module;
}

OwningOpRef<ModuleOp> parseInput(const std::string &path, MLIRContext &context) {
  std::string errorMessage;
  auto file = openInputFile(path, &errorMessage);
  if (!file) {
    error() << path << ": " << errorMessage << "\n";
    return nullptr;
  }
  return parseBuffer(std::move(file), path, context);
}

// ---------------------------------------------------------------------------
// Record definitions
// ---------------------------------------------------------------------------
//
// Named CIR records are uniqued by name in an MLIRContext. If two inputs
// defined the same record name differently and were parsed into one context,
// the second input would silently use the first body (and only fail to verify
// if it happened to access a member). So each input is first parsed in a
// context of its own, the definition of every complete named record is
// fingerprinted there, and the fingerprints are compared across inputs.
//
// Records without a source name are the exception: clang names them `anon.N`
// with a counter per translation unit, so `anon.6` of one input and `anon.6`
// of another are unrelated. Such a record is identified by its structure
// instead. Equal structures get one name across all inputs, different ones
// different names (assignAnonNames), and an input whose names change is
// rebuilt with the new names before it joins the shared context
// (renumberAnonRecords).

std::string printModule(ModuleOp module);
void registerDialects(MLIRContext &context);

// `anon` or `anon.<digits>`: the names clang gives records without a name. A
// source name cannot contain a dot, so these never clash with one.
bool isAnonName(StringRef name) {
  if (name == "anon")
    return true;
  if (!name.consume_front("anon."))
    return false;
  return !name.empty() && llvm::all_of(name, llvm::isDigit);
}

bool isAnonRecord(cir::RecordType record) {
  return record.getName() && isAnonName(record.getName().getValue());
}

void recordBodyKey(cir::RecordType record, llvm::raw_ostream &os,
                   SmallVectorImpl<cir::RecordType> &stack);

// A textual fingerprint of `type`. Named records are referred to by name:
// their bodies are fingerprinted separately, and a record that is incomplete
// in one input and complete in another must not count as a difference of the
// types that point to it. Anonymous records (literal or `anon.N`) are
// expanded, since their names mean nothing across inputs; `stack` breaks the
// cycle of an anonymous record that points to itself.
void typeKey(Type type, llvm::raw_ostream &os,
             SmallVectorImpl<cir::RecordType> &stack) {
  if (auto record = dyn_cast<cir::RecordType>(type)) {
    if (record.getName() && !isAnonRecord(record)) {
      os << "%" << record.getName().getValue();
      return;
    }
    for (auto [depth, open] : llvm::enumerate(stack))
      if (open == record) {
        os << "^" << depth;
        return;
      }
    stack.push_back(record);
    os << "{";
    recordBodyKey(record, os, stack);
    os << "}";
    stack.pop_back();
    return;
  }
  if (auto ptr = dyn_cast<cir::PointerType>(type)) {
    os << "ptr<";
    typeKey(ptr.getPointee(), os, stack);
    if (ptr.getAddrSpace())
      os << "," << ptr.getAddrSpace();
    os << ">";
    return;
  }
  if (auto array = dyn_cast<cir::ArrayType>(type)) {
    os << "array<";
    typeKey(array.getElementType(), os, stack);
    os << " x " << array.getSize() << ">";
    return;
  }
  if (auto func = dyn_cast<cir::FuncType>(type)) {
    os << "func<(";
    for (Type input : func.getInputs()) {
      typeKey(input, os, stack);
      os << ",";
    }
    os << (func.getVarArg() ? "..." : "") << ")->";
    typeKey(func.getReturnType(), os, stack);
    os << ">";
    return;
  }
  type.print(os);
}

void recordBodyKey(cir::RecordType record, llvm::raw_ostream &os,
                   SmallVectorImpl<cir::RecordType> &stack) {
  if (record.isIncomplete()) {
    os << "incomplete";
    return;
  }
  os << (record.isUnion() ? "union" : record.isClass() ? "class" : "struct")
     << " packed=" << record.getPacked() << " padded=" << record.getPadded();
  if (auto u = dyn_cast<cir::UnionType>(record); u && u.getPadding()) {
    os << " padding=";
    typeKey(u.getPadding(), os, stack);
  }
  os << " {";
  for (auto [i, member] : llvm::enumerate(record.getMembers())) {
    os << (i ? ", " : "");
    if (i < record.getMemberKinds().size())
      os << static_cast<unsigned>(record.getMemberKinds()[i]) << ":";
    typeKey(member, os, stack);
  }
  os << "}";
}

std::string recordKey(cir::RecordType record) {
  std::string key;
  llvm::raw_string_ostream os(key);
  SmallVector<cir::RecordType> stack;
  if (isAnonRecord(record))
    stack.push_back(record);
  recordBodyKey(record, os, stack);
  return key;
}

// Every named record type `module` uses, in a deterministic order.
llvm::SetVector<cir::RecordType> collectRecords(ModuleOp module) {
  llvm::SetVector<cir::RecordType> records;
  llvm::DenseSet<Type> seen;
  std::function<void(Type)> visit = [&](Type type) {
    if (!seen.insert(type).second)
      return;
    if (auto record = dyn_cast<cir::RecordType>(type)) {
      if (record.getName())
        records.insert(record);
      // Identified records do not expose their members as sub-elements.
      for (Type member : record.getMembers())
        member.walk([&](Type t) { visit(t); });
      if (auto u = dyn_cast<cir::UnionType>(record); u && u.getPadding())
        u.getPadding().walk([&](Type t) { visit(t); });
    }
  };
  auto visitAll = [&](Type type) { type.walk([&](Type t) { visit(t); }); };
  module->walk([&](Operation *op) {
    for (Type t : op->getResultTypes())
      visitAll(t);
    for (Region &region : op->getRegions())
      for (Block &block : region)
        for (BlockArgument arg : block.getArguments())
          visitAll(arg.getType());
    op->getAttrDictionary().walk([&](Type t) { visit(t); });
    if (Attribute props = op->getPropertiesAsAttribute())
      props.walk([&](Type t) { visit(t); });
  });
  return records;
}

// Gives every anonymous record of every input its name in the merged module:
// a structure seen before keeps the name it got then, a new structure keeps
// its own name if that is still free and gets the next free `anon.N`
// otherwise. Returns, per input, the renames (old name -> new name).
std::vector<std::map<std::string, std::string>>
assignAnonNames(ArrayRef<llvm::SetVector<cir::RecordType>> records) {
  std::map<std::string, std::string> nameOfKey;
  llvm::StringSet<> used;
  unsigned next = 0;
  for (const auto &input : records)
    for (cir::RecordType record : input)
      if (isAnonRecord(record)) {
        StringRef name = record.getName().getValue();
        unsigned n;
        if (name.consume_front("anon.") && !name.getAsInteger(10, n))
          next = std::max(next, n + 1);
      }

  std::vector<std::map<std::string, std::string>> renames(records.size());
  for (size_t i = 0; i < records.size(); ++i)
    for (cir::RecordType record : records[i]) {
      if (!isAnonRecord(record))
        continue;
      std::string name = record.getName().getValue().str();
      std::string key = recordKey(record);
      auto it = nameOfKey.find(key);
      std::string final;
      if (it != nameOfKey.end())
        final = it->second;
      else if (!used.contains(name))
        final = name;
      else
        final = "anon." + std::to_string(next++);
      nameOfKey.emplace(key, final);
      used.insert(final);
      if (final != name)
        renames[i][name] = final;
    }
  return renames;
}

// Record name -> fingerprint of its definition, for the named records of one
// input.
using RecordDefs = std::map<std::string, std::string>;

RecordDefs namedRecordDefs(const llvm::SetVector<cir::RecordType> &records) {
  RecordDefs defs;
  for (cir::RecordType record : records)
    if (!isAnonRecord(record) && record.isComplete())
      defs[record.getName().getValue().str()] = recordKey(record);
  return defs;
}

// Returns false after printing every record name that two inputs define
// differently.
bool checkRecordDefs(ArrayRef<std::string> paths, ArrayRef<RecordDefs> defs) {
  bool ok = true;
  llvm::StringMap<size_t> firstInput;
  for (size_t i = 0; i < defs.size(); ++i)
    for (auto &[name, key] : defs[i]) {
      auto [it, inserted] = firstInput.insert({name, i});
      if (inserted)
        continue;
      const std::string &firstKey = defs[it->second].at(name);
      if (firstKey == key)
        continue;
      error() << paths[i] << ": record '" << name
              << "' is defined differently than in " << paths[it->second]
              << "\n";
      note() << paths[it->second] << ": " << firstKey << "\n";
      note() << paths[i] << ": " << key << "\n";
      note() << "CIR identifies records by name, so cir-merge cannot merge "
             << "translation units that give one name two definitions\n";
      ok = false;
    }
  return ok;
}

// MLIR's generic sub-element rebuild cannot rebuild a cir::FuncType without a
// return type (it is stored as a null type), so the replacer rebuilds function
// types itself.
void addFuncTypeReplacement(AttrTypeReplacer &replacer) {
  replacer.addReplacement(
      [&replacer](cir::FuncType func)
          -> std::optional<std::pair<Type, WalkResult>> {
        SmallVector<Type> inputs;
        for (Type input : func.getInputs())
          inputs.push_back(replacer.replace(input));
        Type result = func.getOptionalReturnType();
        if (result)
          result = replacer.replace(result);
        return std::make_pair(
            cir::FuncType::get(func.getContext(), inputs, result,
                               func.getVarArg()),
            WalkResult::skip());
      });
}

// Replaces, everywhere in `module`, each record in `newName` by a record of
// the same body under the new name. Records are immutable once complete, so
// every record whose body (transitively) contains a replaced record is
// replaced as well; `newName` must already include those. The new names must
// be free in the module's context.
void replaceRecords(ModuleOp module,
                    const llvm::MapVector<cir::RecordType, std::string> &newName) {
  MLIRContext *context = module.getContext();
  // Two records with the same structure can get the same new name; the
  // first one defines it.
  llvm::DenseMap<Type, Type> replacement;
  llvm::StringMap<Type> created;
  SmallVector<cir::RecordType> definers;
  for (auto &[record, name] : newName) {
    auto [it, inserted] = created.try_emplace(name);
    if (inserted) {
      StringAttr nameAttr = StringAttr::get(context, name);
      if (isa<cir::UnionType>(record))
        it->second = cir::UnionType::get(context, nameAttr);
      else
        it->second = cir::StructType::get(context, nameAttr, record.isClass());
      definers.push_back(record);
    }
    replacement[record] = it->second;
  }
  AttrTypeReplacer replacer;
  replacer.addReplacement([&](cir::RecordType record) -> std::optional<Type> {
    auto it = replacement.find(record);
    if (it == replacement.end())
      return std::nullopt;
    return it->second;
  });
  addFuncTypeReplacement(replacer);
  for (cir::RecordType record : definers) {
    if (record.isIncomplete())
      continue;
    SmallVector<Type> members;
    for (Type member : record.getMembers())
      members.push_back(replacer.replace(member));
    if (auto u = dyn_cast<cir::UnionType>(record)) {
      Type padding = u.getPadding() ? replacer.replace(u.getPadding()) : Type();
      cast<cir::UnionType>(replacement[record])
          .complete(members, u.getPacked(), padding, u.getMemberKinds());
    } else {
      cast<cir::StructType>(replacement[record])
          .complete(members, record.getPacked(), record.getMemberKinds());
    }
  }
  replacer.recursivelyReplaceElementsIn(module, /*replaceAttrs=*/true,
                                        /*replaceLocs=*/false,
                                        /*replaceTypes=*/true);
}

// Rebuilds `module` with the anonymous records renamed by `renames`, and
// returns its text. The old and the new names can overlap (anon.6 -> anon.9
// while anon.9 -> anon.10), and a record name cannot be reused in a context,
// so the records go through temporary names in a second context: the module
// is rewritten to the temporary names, printed, parsed into a fresh context
// where the final names are free, and rewritten to them there.
std::optional<std::string>
renumberAnonRecords(ModuleOp module, const std::string &path,
                    const llvm::SetVector<cir::RecordType> &records,
                    const std::map<std::string, std::string> &renames) {
  const std::string tempPrefix = "cir-merge.renumber.";

  // Every record that contains a renamed one, directly or through a pointer,
  // array or function type, must be rebuilt too.
  llvm::DenseSet<Type> affected;
  for (cir::RecordType record : records)
    if (isAnonRecord(record) &&
        renames.count(record.getName().getValue().str()))
      affected.insert(record);
  bool changed = true;
  while (changed) {
    changed = false;
    for (cir::RecordType record : records) {
      if (affected.contains(record))
        continue;
      bool reaches = false;
      for (Type member : record.getMembers())
        member.walk([&](Type t) {
          if (affected.contains(t))
            reaches = true;
        });
      if (reaches) {
        affected.insert(record);
        changed = true;
      }
    }
  }

  llvm::MapVector<cir::RecordType, std::string> toTemp;
  for (cir::RecordType record : records)
    if (affected.contains(record)) {
      std::string name = record.getName().getValue().str();
      auto it = renames.find(name);
      toTemp[record] = tempPrefix + (it != renames.end() ? it->second : name);
    }
  replaceRecords(module, toTemp);

  MLIRContext fresh;
  registerDialects(fresh);
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(
      llvm::MemoryBuffer::getMemBufferCopy(printModule(module),
                                           path + " (records renumbered)"),
      llvm::SMLoc());
  SourceMgrDiagnosticHandler handler(sourceMgr, &fresh);
  ParserConfig config(&fresh, /*verifyAfterParse=*/true);
  OwningOpRef<ModuleOp> rebuilt = parseSourceFile<ModuleOp>(sourceMgr, config);
  if (!rebuilt) {
    error() << path << ": the module with renumbered records does not parse\n";
    return std::nullopt;
  }

  llvm::StringSet<> replacedNames;
  for (auto &entry : toTemp)
    replacedNames.insert(entry.first.getName().getValue());
  llvm::MapVector<cir::RecordType, std::string> toFinal;
  for (cir::RecordType record : collectRecords(*rebuilt)) {
    StringRef name = record.getName().getValue();
    if (name.consume_front(tempPrefix)) {
      toFinal[record] = name.str();
    } else if (replacedNames.contains(name)) {
      error() << path << ": record '" << name
              << "' is still used after renumbering (internal error)\n";
      return std::nullopt;
    }
  }
  replaceRecords(*rebuilt, toFinal);

  // cir.record_layouts is keyed by record name.
  if (auto layouts = (*rebuilt)->getAttrOfType<DictionaryAttr>(
          "cir.record_layouts")) {
    llvm::MapVector<StringAttr, Attribute> renamed;
    for (NamedAttribute entry : layouts) {
      auto it = renames.find(entry.getName().getValue().str());
      StringAttr name = it == renames.end()
                            ? entry.getName()
                            : StringAttr::get(&fresh, it->second);
      renamed.insert({name, entry.getValue()});
    }
    SmallVector<NamedAttribute> entries;
    for (auto &[name, value] : renamed)
      entries.push_back(NamedAttribute(name, value));
    (*rebuilt)->setAttr("cir.record_layouts",
                        DictionaryAttr::get(&fresh, entries));
  }
  return printModule(*rebuilt);
}

// ---------------------------------------------------------------------------
// Module compatibility
// ---------------------------------------------------------------------------
//
// Every module attribute clang's CIR emission produces at the pinned revision
// is classified here. An attribute not listed is rejected, because its effect
// on a merge is not known.

// Attributes that describe the target, the ABI or the language. They must be
// identical (present in all inputs with the same value, or absent in all).
const char *const kMustMatchAttrs[] = {
    "cir.triple",          "dlti.dl_spec",          "cir.lang",
    "cir.int_type_width",  "cir.size_type_width",   "cir.default_tls_model",
};

// Inline assembly at module level. cir-merge only accepts it when it is empty
// everywhere: concatenating asm blocks may duplicate symbol definitions.
const char *const kModuleAsmAttr = "cir.module_asm";

// The record layouts clang computed, keyed by record name. Merged as a union;
// a record present in several inputs must have the same layout in all.
const char *const kRecordLayoutsAttr = "cir.record_layouts";

// The static constructors and destructors of each translation unit. Merged by
// concatenation in input order; the order between translation units is
// unspecified in C++ anyway.
const char *const kGlobalCtorsAttr = "cir.global_ctors";
const char *const kGlobalDtorsAttr = "cir.global_dtors";

bool isKnownModuleAttr(StringRef name) {
  for (const char *a : kMustMatchAttrs)
    if (name == a)
      return true;
  return name == kModuleAsmAttr || name == kRecordLayoutsAttr ||
         name == kGlobalCtorsAttr || name == kGlobalDtorsAttr ||
         name == "sym_name";
}

// Returns the merged module attributes, or nullopt after printing why the
// inputs cannot be merged.
std::optional<SmallVector<NamedAttribute>>
mergeModuleAttributes(std::vector<Input> &inputs, MLIRContext &context) {
  bool ok = true;

  for (Input &in : inputs)
    for (NamedAttribute attr : in.module.get()->getAttrs())
      if (!isKnownModuleAttr(attr.getName().getValue())) {
        error() << in.path << ": unsupported module attribute '"
                << attr.getName().getValue() << "'\n";
        note() << "cir-merge does not know whether this attribute allows the "
               << "modules to be combined, so it rejects the input\n";
        ok = false;
      }

  SmallVector<NamedAttribute> merged;
  ModuleOp first = *inputs.front().module;

  for (const char *name : kMustMatchAttrs) {
    Attribute ref = first->getAttr(name);
    for (Input &in : inputs) {
      Attribute attr = in.module.get()->getAttr(name);
      if (attr == ref)
        continue;
      error() << in.path << ": module attribute '" << name
              << "' differs from the one in " << inputs.front().path << "\n";
      note() << inputs.front().path << ": " << name << " = ";
      if (ref)
        ref.print(llvm::errs());
      else
        llvm::errs() << "<absent>";
      llvm::errs() << "\n";
      note() << in.path << ": " << name << " = ";
      if (attr)
        attr.print(llvm::errs());
      else
        llvm::errs() << "<absent>";
      llvm::errs() << "\n";
      note() << "translation units built for different targets, ABIs or "
             << "languages cannot be merged\n";
      ok = false;
      break;
    }
    if (ref)
      merged.push_back(NamedAttribute(StringAttr::get(&context, name), ref));
  }

  bool anyAsm = false;
  for (Input &in : inputs) {
    Attribute attr = in.module.get()->getAttr(kModuleAsmAttr);
    if (!attr)
      continue;
    anyAsm = true;
    auto array = dyn_cast<ArrayAttr>(attr);
    if (!array || !array.empty()) {
      error() << in.path << ": module-level inline assembly is not supported\n";
      note() << "cir-merge does not merge 'cir.module_asm'\n";
      ok = false;
    }
  }
  if (anyAsm)
    merged.push_back(NamedAttribute(StringAttr::get(&context, kModuleAsmAttr),
                                    ArrayAttr::get(&context, {})));

  llvm::MapVector<StringAttr, std::pair<Attribute, std::string>> layouts;
  bool anyLayouts = false;
  for (Input &in : inputs) {
    Attribute attr = in.module.get()->getAttr(kRecordLayoutsAttr);
    if (!attr)
      continue;
    anyLayouts = true;
    auto dict = dyn_cast<DictionaryAttr>(attr);
    if (!dict) {
      error() << in.path << ": '" << kRecordLayoutsAttr
              << "' is not a dictionary\n";
      ok = false;
      continue;
    }
    for (NamedAttribute entry : dict) {
      auto [it, inserted] =
          layouts.insert({entry.getName(), {entry.getValue(), in.path}});
      if (!inserted && it->second.first != entry.getValue()) {
        error() << in.path << ": record '" << entry.getName().getValue()
                << "' has a different layout than in " << it->second.second
                << "\n";
        note() << "two translation units define the same record name "
               << "differently; cir-merge cannot merge them\n";
        ok = false;
      }
    }
  }
  if (anyLayouts) {
    SmallVector<NamedAttribute> entries;
    for (auto &[name, value] : layouts)
      entries.push_back(NamedAttribute(name, value.first));
    merged.push_back(NamedAttribute(StringAttr::get(&context, kRecordLayoutsAttr),
                                    DictionaryAttr::get(&context, entries)));
  }

  for (const char *name : {kGlobalCtorsAttr, kGlobalDtorsAttr}) {
    SmallVector<Attribute> entries;
    bool any = false;
    for (Input &in : inputs) {
      Attribute attr = in.module.get()->getAttr(name);
      if (!attr)
        continue;
      any = true;
      auto array = dyn_cast<ArrayAttr>(attr);
      if (!array) {
        error() << in.path << ": '" << name << "' is not an array\n";
        ok = false;
        continue;
      }
      llvm::append_range(entries, array.getValue());
    }
    if (any)
      merged.push_back(NamedAttribute(StringAttr::get(&context, name),
                                      ArrayAttr::get(&context, entries)));
  }

  if (!ok)
    return std::nullopt;
  return merged;
}

// ---------------------------------------------------------------------------
// Symbol inventory
// ---------------------------------------------------------------------------

enum class SymKind { Function, Global };

struct Symbol {
  unsigned input;    // index into `inputs`
  Operation *op;
  SymKind kind;
  StringRef name;
  bool isDefinition;
  cir::GlobalLinkageKind linkage;
};

StringRef kindName(SymKind kind) {
  return kind == SymKind::Function ? "function" : "global";
}

// A function is a definition when it has a body or is an alias of another
// function (clang emits complete-object constructors as aliases of the base
// constructor). Anything else is a declaration.
bool isFunctionDefinition(cir::FuncOp func) {
  return !func.getBody().empty() || func.getAliasee().has_value();
}

// A global is a definition when it has an initial value or an initialization
// region. `cir.global "private" external @x : T` is a declaration.
bool isGlobalDefinition(cir::GlobalOp global) {
  return global.getInitialValue().has_value() ||
         !global.getCtorRegion().empty() || !global.getDtorRegion().empty() ||
         global.getAliasee().has_value();
}

bool isTranslationUnitLocal(cir::GlobalLinkageKind linkage) {
  return linkage == cir::GlobalLinkageKind::InternalLinkage ||
         linkage == cir::GlobalLinkageKind::PrivateLinkage;
}

// ---------------------------------------------------------------------------
// String-named references and the renaming of translation-unit-local symbols
// ---------------------------------------------------------------------------
//
// Most references to a symbol are SymbolRefAttrs. MLIR finds and rewrites
// those itself, also inside other attributes (the callee of cir.call, the
// symbol of cir.get_global, #cir.global_view inside #cir.vtable or
// #cir.typeinfo, the function of #cir.block_addr_info, a personality). CIR also
// names symbols with plain strings, which MLIR does not see. At the pinned
// revision these are:
//
//   module  cir.global_ctors, cir.global_dtors  #cir.global_ctor<"fn", prio>
//   global  static_local_guard<"guard">         the guard variable of a
//                                               function-local static
//   global  tls_refs = <"wrapper", "init", ...> the thread_local wrapper,
//                                               initializer and guard
//
// cir-merge rewrites these with the symbols they name. The CUDA attributes
// that name kernels by string are not handled, and inputs carrying them are
// rejected.

std::optional<cir::GlobalLinkageKind> linkageOf(Operation *op) {
  if (auto func = dyn_cast<cir::FuncOp>(op))
    return func.getLinkage();
  if (auto global = dyn_cast<cir::GlobalOp>(op))
    return global.getLinkage();
  return std::nullopt;
}

// Calls `fn` with every string-named symbol reference of `module`.
void forEachStringRef(ModuleOp module, llvm::function_ref<void(StringAttr)> fn) {
  for (const char *attrName : {kGlobalCtorsAttr, kGlobalDtorsAttr})
    if (auto list = module->getAttrOfType<ArrayAttr>(attrName))
      for (Attribute entry : list) {
        if (auto ctor = dyn_cast<cir::GlobalCtorAttr>(entry))
          fn(ctor.getName());
        else if (auto dtor = dyn_cast<cir::GlobalDtorAttr>(entry))
          fn(dtor.getName());
      }
  for (auto global : module.getOps<cir::GlobalOp>()) {
    if (auto guard = global.getStaticLocalGuardAttr())
      fn(guard.getName());
    if (auto tls = global.getTlsRefsAttr())
      for (StringAttr name :
           {tls.getWrapperName(), tls.getInitName(), tls.getGuardName()})
        if (name && !name.getValue().empty())
          fn(name);
  }
}

// Rewrites the string-named references of `module` according to `renames`.
void renameStringRefs(ModuleOp module,
                      const llvm::StringMap<std::string> &renames) {
  MLIRContext *context = module.getContext();
  auto renamed = [&](StringAttr name) -> StringAttr {
    if (!name)
      return name;
    auto it = renames.find(name.getValue());
    return it == renames.end() ? name : StringAttr::get(context, it->second);
  };
  for (const char *attrName : {kGlobalCtorsAttr, kGlobalDtorsAttr}) {
    auto list = module->getAttrOfType<ArrayAttr>(attrName);
    if (!list)
      continue;
    SmallVector<Attribute> entries;
    for (Attribute entry : list) {
      if (auto ctor = dyn_cast<cir::GlobalCtorAttr>(entry))
        entry = cir::GlobalCtorAttr::get(renamed(ctor.getName()),
                                         ctor.getPriority());
      else if (auto dtor = dyn_cast<cir::GlobalDtorAttr>(entry))
        entry = cir::GlobalDtorAttr::get(renamed(dtor.getName()),
                                         dtor.getPriority());
      entries.push_back(entry);
    }
    module->setAttr(attrName, ArrayAttr::get(context, entries));
  }
  for (auto global : module.getOps<cir::GlobalOp>()) {
    if (auto guard = global.getStaticLocalGuardAttr())
      global.setStaticLocalGuardAttr(
          cir::StaticLocalGuardAttr::get(context, renamed(guard.getName())));
    if (auto tls = global.getTlsRefsAttr())
      global.setTlsRefsAttr(cir::ThreadLocalGlobalWrapperInitAttr::get(
          context, renamed(tls.getWrapperName()), renamed(tls.getInitName()),
          renamed(tls.getGuardName())));
  }
}

// Gives every translation-unit-local symbol (internal or private linkage) whose
// name another input also uses a new name that no input uses, and rewrites the
// references to it inside its own input. Two inputs' local symbols are
// distinct entities, and so is a local symbol and another input's external
// symbol of the same name; after the renaming they no longer collide.
//
// The new name is `<name>.tu<N>`, N being the 1-based position of the input
// on the command line (with a further `.<k>` if that name is taken). A name is
// "used" by an input when the input defines or declares it, or names it in a
// string-named reference.
bool renameLocalCollisions(std::vector<Input> &inputs) {
  llvm::StringMap<unsigned> usedBy; // name -> number of inputs using it
  llvm::StringSet<> taken;
  for (Input &in : inputs) {
    llvm::StringSet<> mine;
    for (Operation &op : in.module->getBody()->getOperations())
      if (auto name = op.getAttrOfType<StringAttr>("sym_name"))
        mine.insert(name.getValue());
    forEachStringRef(*in.module,
                     [&](StringAttr name) { mine.insert(name.getValue()); });
    for (auto &entry : mine) {
      ++usedBy[entry.getKey()];
      taken.insert(entry.getKey());
    }
  }

  bool ok = true;
  for (unsigned i = 0; i < inputs.size(); ++i) {
    ModuleOp module = *inputs[i].module;
    MLIRContext *context = module.getContext();
    llvm::StringMap<std::string> renames;
    for (Operation &op : module.getBody()->getOperations()) {
      std::optional<cir::GlobalLinkageKind> linkage = linkageOf(&op);
      if (!linkage || !isTranslationUnitLocal(*linkage))
        continue;
      StringRef name = op.getAttrOfType<StringAttr>("sym_name").getValue();
      if (usedBy.lookup(name) < 2)
        continue;
      std::string base = (name + ".tu" + Twine(i + 1)).str();
      std::string fresh = base;
      for (unsigned k = 1; taken.contains(fresh); ++k)
        fresh = base + "." + std::to_string(k);
      taken.insert(fresh);
      StringAttr freshAttr = StringAttr::get(context, fresh);
      if (failed(SymbolTable::replaceAllSymbolUses(&op, freshAttr, module))) {
        error() << inputs[i].path << ": cannot rewrite the uses of @" << name
                << " to rename it\n";
        ok = false;
        continue;
      }
      if (verbose)
        llvm::errs() << "cir-merge: @" << name << " ("
                     << cir::stringifyGlobalLinkageKind(*linkage) << " in "
                     << inputs[i].path << ") renamed to @" << fresh << "\n";
      renames[name] = fresh;
      SymbolTable::setSymbolName(&op, freshAttr);
    }
    if (!renames.empty())
      renameStringRefs(module, renames);
  }
  return ok;
}

// Collects every top-level operation of every input. Returns false after
// printing why an operation is outside the supported subset.
bool buildInventory(std::vector<Input> &inputs, std::vector<Symbol> &symbols) {
  bool ok = true;
  for (unsigned i = 0; i < inputs.size(); ++i) {
    for (Operation &op : inputs[i].module->getBody()->getOperations()) {
      bool cuda = false;
      op.getAttrDictionary().walk([&](Attribute attr) {
        if (isa<cir::CUDAKernelNameAttr, cir::CUDABinaryHandleAttr>(attr))
          cuda = true;
      });
      if (cuda) {
        error() << inputs[i].path << ": @"
                << op.getAttrOfType<StringAttr>("sym_name").getValue()
                << " carries a CUDA attribute that names a symbol by string\n";
        note() << "cir-merge does not rewrite CUDA kernel references\n";
        ok = false;
        continue;
      }
      if (auto func = dyn_cast<cir::FuncOp>(op)) {
        symbols.push_back({i, &op, SymKind::Function, func.getSymName(),
                           isFunctionDefinition(func), func.getLinkage()});
        continue;
      }
      if (auto global = dyn_cast<cir::GlobalOp>(op)) {
        symbols.push_back({i, &op, SymKind::Global, global.getSymName(),
                           isGlobalDefinition(global), global.getLinkage()});
        continue;
      }
      // Not a function or global: its cross-module meaning is unknown, whether
      // or not it defines a symbol, so it is rejected rather than copied.
      error() << inputs[i].path << ": unsupported top-level operation '"
              << op.getName() << "'";
      if (auto name = op.getAttrOfType<StringAttr>(
              "sym_name"))
        llvm::errs() << " defining symbol @" << name.getValue();
      llvm::errs() << "\n";
      note() << "at " << locString(op.getLoc()) << "\n";
      note() << "cir-merge merges only 'cir.func' and 'cir.global'\n";
      ok = false;
    }
  }
  return ok;
}

void printInventory(const std::vector<Input> &inputs,
                    const std::vector<Symbol> &symbols) {
  llvm::outs() << "# input\tkind\tdef/decl\tlinkage\tsymbol\n";
  for (const Symbol &s : symbols)
    llvm::outs() << inputs[s.input].path << "\t" << kindName(s.kind) << "\t"
                 << (s.isDefinition ? "definition" : "declaration") << "\t"
                 << cir::stringifyGlobalLinkageKind(s.linkage) << "\t@"
                 << s.name << "\n";
}

// ---------------------------------------------------------------------------
// Merge planning
// ---------------------------------------------------------------------------
//
// Symbols are grouped by name across all inputs before anything is decided,
// so the outcome does not depend on the order of the inputs. Rules:
//
//   one occurrence                          keep it
//   only compatible declarations            keep the first declaration
//   compatible declarations + 1 definition  keep the definition
//   two or more definitions                 reject (no ODR / COMDAT / weak
//                                           selection)
//   function vs global with the same name   reject
//   internal or private linkage colliding   cannot happen: such symbols were
//                                           renamed (renameLocalCollisions);
//                                           rejected as an internal error
//   incompatible types or attributes        reject
//   unsupported linkage in a collision      reject

// Linkages a definition may have when declarations of it come from other
// inputs. The definition is the only one in the program, so the rule "keep
// the definition" gives it the meaning the linker would.
bool definitionLinkageMergeable(cir::GlobalLinkageKind linkage) {
  switch (linkage) {
  case cir::GlobalLinkageKind::ExternalLinkage:
  case cir::GlobalLinkageKind::LinkOnceODRLinkage:
  case cir::GlobalLinkageKind::WeakODRLinkage:
  case cir::GlobalLinkageKind::AvailableExternallyLinkage:
    return true;
  default:
    return false;
  }
}

// Returns why `b` cannot be merged with `a` (both share a name and a kind),
// or an empty string when they are compatible.
std::string incompatibility(const Symbol &a, const Symbol &b) {
  if (a.kind == SymKind::Function) {
    auto fa = cast<cir::FuncOp>(a.op), fb = cast<cir::FuncOp>(b.op);
    if (fa.getFunctionType() != fb.getFunctionType()) {
      std::string s;
      llvm::raw_string_ostream os(s);
      os << "function types differ: " << fa.getFunctionType() << " vs "
         << fb.getFunctionType();
      return s;
    }
    if (fa.getNoProto() != fb.getNoProto())
      return "one is declared without a prototype and the other with one";
    if (fa.getCallingConv() != fb.getCallingConv())
      return "calling conventions differ";
    return "";
  }
  auto ga = cast<cir::GlobalOp>(a.op), gb = cast<cir::GlobalOp>(b.op);
  if (ga.getSymType() != gb.getSymType()) {
    std::string s;
    llvm::raw_string_ostream os(s);
    os << "types differ: " << ga.getSymType() << " vs " << gb.getSymType();
    return s;
  }
  if (ga.getConstant() != gb.getConstant())
    return "one is constant and the other is not";
  if (ga.getTlsModelAttr() != gb.getTlsModelAttr())
    return "thread-local storage models differ";
  if (ga.getAddrSpaceAttr() != gb.getAddrSpaceAttr())
    return "address spaces differ";
  return "";
}

// ---------------------------------------------------------------------------
// Equivalence of ODR definitions
// ---------------------------------------------------------------------------
//
// C++ lets every translation unit that uses an inline function, a template
// instance, a vtable or a typeinfo object emit its own definition, with
// linkonce_odr (or weak_odr) linkage; the One Definition Rule promises that
// all of them are the same, and the linker keeps one. cir-merge keeps one only
// when it can see that they are the same: two definitions are equivalent when
// they are structurally equal, ignoring source locations, where a reference
// to a translation-unit-local symbol (a string literal, a static helper)
// matches a reference to another input's local symbol when those two local
// symbols are equivalent in turn. Differing definitions are an error: either
// the program breaks the ODR, or the translation units were compiled with
// different options.

class OdrComparator {
public:
  explicit OdrComparator(std::vector<Input> &inputs) : inputs(inputs) {
    for (Input &in : inputs)
      tables.emplace_back(std::make_unique<SymbolTable>(*in.module));
  }

  bool equivalent(Operation *a, unsigned inputA, Operation *b, unsigned inputB) {
    // Coinductive: a pair under comparison counts as equivalent, so that
    // mutually recursive local functions compare as equal when their bodies
    // are.
    if (!assumed.insert({a, b}).second)
      return true;
    if (a->getName() != b->getName())
      return false;
    std::vector<Operation *> localsA, localsB;
    Operation *canonA = canonicalClone(a, inputA, localsA);
    Operation *canonB = canonicalClone(b, inputB, localsB);
    bool same = OperationEquivalence::isEquivalentTo(
        canonA, canonB, OperationEquivalence::IgnoreLocations);
    canonA->destroy();
    canonB->destroy();
    if (!same || localsA.size() != localsB.size())
      return false;
    for (size_t i = 0; i < localsA.size(); ++i)
      if (!equivalent(localsA[i], inputA, localsB[i], inputB))
        return false;
    return true;
  }

private:
  // A copy of `op` in which the k-th distinct translation-unit-local symbol
  // it references (in traversal order) is renamed to a placeholder for k; the
  // referenced local symbols are returned in `locals`.
  Operation *canonicalClone(Operation *op, unsigned input,
                            std::vector<Operation *> &locals) {
    Operation *clone = op->clone();
    MLIRContext *context = op->getContext();
    // The names differ for two local symbols, and the linkage, COMDAT and
    // dso_local flags may differ between equivalent ODR definitions (weak_odr
    // for an explicit instantiation, available_externally for an extern
    // template); none of them is part of what the definition means.
    SymbolTable::setSymbolName(clone, "\x01cir-merge-self");
    if (auto func = dyn_cast<cir::FuncOp>(clone)) {
      func.setLinkage(cir::GlobalLinkageKind::LinkOnceODRLinkage);
      func.setComdat(false);
      func.setDsoLocal(false);
    } else if (auto global = dyn_cast<cir::GlobalOp>(clone)) {
      global.setLinkage(cir::GlobalLinkageKind::LinkOnceODRLinkage);
      global.setComdat(false);
      global.setDsoLocal(false);
    }
    llvm::DenseMap<StringAttr, unsigned> index;
    AttrTypeReplacer replacer;
    replacer.addReplacement(
        [&](SymbolRefAttr ref) -> std::optional<Attribute> {
          Operation *target = tables[input]->lookup(ref.getRootReference());
          if (!target)
            return std::nullopt;
          std::optional<cir::GlobalLinkageKind> linkage = linkageOf(target);
          if (!linkage || !isTranslationUnitLocal(*linkage))
            return std::nullopt;
          auto [it, inserted] =
              index.insert({ref.getRootReference(), locals.size()});
          if (inserted)
            locals.push_back(target);
          return FlatSymbolRefAttr::get(
              context, "\x01cir-merge-local-" + std::to_string(it->second));
        });
    replacer.recursivelyReplaceElementsIn(clone, /*replaceAttrs=*/true,
                                          /*replaceLocs=*/false,
                                          /*replaceTypes=*/false);
    return clone;
  }

  std::vector<Input> &inputs;
  std::vector<std::unique_ptr<SymbolTable>> tables;
  std::set<std::pair<Operation *, Operation *>> assumed;
};

bool isOdrLinkage(cir::GlobalLinkageKind linkage) {
  return linkage == cir::GlobalLinkageKind::LinkOnceODRLinkage ||
         linkage == cir::GlobalLinkageKind::WeakODRLinkage ||
         linkage == cir::GlobalLinkageKind::AvailableExternallyLinkage;
}

struct Plan {
  llvm::DenseSet<Operation *> keep;
};

void explain(const std::vector<Input> &inputs, const Symbol &s,
             StringRef what) {
  if (verbose)
    llvm::errs() << "cir-merge: @" << s.name << ": " << what << " ("
                 << kindName(s.kind) << " from " << inputs[s.input].path
                 << ")\n";
}

bool planMerge(std::vector<Input> &inputs, const std::vector<Symbol> &symbols,
               Plan &plan) {
  OdrComparator odr(inputs);
  llvm::MapVector<StringRef, SmallVector<const Symbol *, 2>> byName;
  for (const Symbol &s : symbols)
    byName[s.name].push_back(&s);

  bool ok = true;
  auto where = [&](const Symbol &s) {
    return inputs[s.input].path + " (" + locString(s.op->getLoc()) + ")";
  };

  for (auto &[name, group] : byName) {
    if (group.size() == 1) {
      plan.keep.insert(group.front()->op);
      explain(inputs, *group.front(), "unique, kept");
      continue;
    }

    const Symbol &first = *group.front();
    bool groupOk = true;

    // A symbol name that is defined more than once inside one input is not
    // something clang emits; refuse it rather than guess.
    for (size_t i = 1; i < group.size() && groupOk; ++i)
      for (size_t j = 0; j < i; ++j)
        if (group[i]->input == group[j]->input) {
          error() << inputs[group[i]->input].path << ": symbol @" << name
                  << " occurs more than once in the same module\n";
          groupOk = false;
          break;
        }

    for (const Symbol *s : group) {
      if (!groupOk)
        break;
      if (s->kind != first.kind) {
        error() << inputs[s->input].path << ": symbol @" << name << " is a "
                << kindName(s->kind) << " here but a " << kindName(first.kind)
                << " in " << inputs[first.input].path << "\n";
        note() << where(*s) << " vs " << where(first) << "\n";
        groupOk = false;
        break;
      }
      if (isTranslationUnitLocal(s->linkage)) {
        const Symbol &other = s == &first ? *group[1] : first;
        error() << inputs[s->input].path << ": symbol @" << name << " has "
                << cir::stringifyGlobalLinkageKind(s->linkage)
                << " linkage and its name is also used in "
                << inputs[other.input].path << "\n";
        note() << "translation-unit-local symbols are distinct entities; "
               << "merging them by name would be wrong (internal error: they "
               << "should have been renamed)\n";
        groupOk = false;
        break;
      }
    }

    SmallVector<const Symbol *, 2> defs;
    if (groupOk)
      for (const Symbol *s : group)
        if (s->isDefinition)
          defs.push_back(s);

    const Symbol *chosen = defs.empty() ? &first : defs.front();

    bool allOdr = llvm::all_of(
        defs, [](const Symbol *d) { return isOdrLinkage(d->linkage); });
    if (groupOk && defs.size() > 1 && allOdr) {
      for (size_t j = 1; j < defs.size(); ++j) {
        if (odr.equivalent(defs[0]->op, defs[0]->input, defs[j]->op,
                           defs[j]->input))
          continue;
        error() << inputs[defs[j]->input].path << ": the definition of "
                << kindName(defs[j]->kind) << " @" << name << " ("
                << cir::stringifyGlobalLinkageKind(defs[j]->linkage)
                << " linkage) differs from the one in "
                << inputs[defs[0]->input].path << "\n";
        note() << where(*defs[j]) << " vs " << where(*defs[0]) << "\n";
        note() << "the One Definition Rule requires them to be the same: the "
               << "program breaks it, or the translation units were compiled "
               << "with different options\n";
        groupOk = false;
        break;
      }
      // Keep the strongest of the equivalent definitions: weak_odr (an
      // explicit instantiation), else linkonce_odr, else available_externally.
      auto rank = [](cir::GlobalLinkageKind linkage) {
        return linkage == cir::GlobalLinkageKind::WeakODRLinkage       ? 0
               : linkage == cir::GlobalLinkageKind::LinkOnceODRLinkage ? 1
                                                                       : 2;
      };
      for (const Symbol *d : defs)
        if (rank(d->linkage) < rank(chosen->linkage))
          chosen = d;
      if (groupOk)
        explain(inputs, *chosen,
                std::to_string(defs.size()) +
                    " equivalent ODR definitions, this one kept");
    } else if (groupOk && defs.size() > 1) {
      error() << inputs[defs[1]->input].path << ": duplicate definition of "
              << kindName(defs[1]->kind) << " @" << name << " ("
              << cir::stringifyGlobalLinkageKind(defs[1]->linkage)
              << " linkage)\n";
      note() << "first definition in " << where(*defs[0]) << "\n";
      if (defs[0]->linkage == cir::GlobalLinkageKind::ExternalLinkage &&
          defs[1]->linkage == cir::GlobalLinkageKind::ExternalLinkage)
        note() << "the program defines this symbol in two translation units\n";
      else
        note() << "only definitions that all have linkonce_odr, weak_odr or "
               << "available_externally linkage can be merged\n";
      groupOk = false;
    }

    if (groupOk && !defs.empty() &&
        !definitionLinkageMergeable(chosen->linkage)) {
      error() << inputs[chosen->input].path << ": definition of @" << name
              << " has " << cir::stringifyGlobalLinkageKind(chosen->linkage)
              << " linkage, which cir-merge does not merge with declarations "
              << "from other translation units\n";
      groupOk = false;
    }

    if (groupOk)
      for (const Symbol *s : group) {
        if (s->isDefinition)
          continue;
        if (s->linkage != cir::GlobalLinkageKind::ExternalLinkage) {
          error() << inputs[s->input].path << ": declaration of @" << name
                  << " has " << cir::stringifyGlobalLinkageKind(s->linkage)
                  << " linkage, which cir-merge does not merge\n";
          groupOk = false;
          break;
        }
      }

    if (groupOk)
      for (const Symbol *s : group) {
        if (s == chosen)
          continue;
        std::string why = incompatibility(*chosen, *s);
        if (why.empty())
          continue;
        error() << inputs[s->input].path << ": " << kindName(s->kind) << " @"
                << name << " is incompatible with its "
                << (chosen->isDefinition ? "definition" : "declaration")
                << " in " << inputs[chosen->input].path << ": " << why << "\n";
        note() << where(*s) << " vs " << where(*chosen) << "\n";
        groupOk = false;
        break;
      }

    if (!groupOk) {
      ok = false;
      continue;
    }
    plan.keep.insert(chosen->op);
    explain(inputs, *chosen,
            chosen->isDefinition
                ? "definition kept, " + std::to_string(group.size() - 1) +
                      " declaration(s) dropped"
                : "declaration kept, " + std::to_string(group.size() - 1) +
                      " duplicate declaration(s) dropped");
  }
  return ok;
}

// ---------------------------------------------------------------------------
// Applying the plan and validating the result
// ---------------------------------------------------------------------------

OwningOpRef<ModuleOp> applyPlan(std::vector<Input> &inputs, const Plan &plan,
                                ArrayRef<NamedAttribute> moduleAttrs,
                                MLIRContext &context) {
  OwningOpRef<ModuleOp> merged = ModuleOp::create(UnknownLoc::get(&context));
  for (NamedAttribute attr : moduleAttrs)
    merged.get()->setAttr(attr.getName(), attr.getValue());
  Block *body = merged->getBody();
  // Moving keeps attributes, regions and locations exactly as parsed; the
  // inputs are not used again.
  for (Input &in : inputs)
    for (Operation &op :
         llvm::make_early_inc_range(in.module->getBody()->getOperations()))
      if (plan.keep.contains(&op)) {
        op.remove();
        body->push_back(&op);
      }
  return merged;
}

// Checks that every symbol reference in the merged module resolves, including
// the string-named references the verifier does not see (the static
// constructor and destructor lists).
bool checkReferences(ModuleOp merged) {
  bool ok = true;
  SymbolTable table(merged);
  auto uses = SymbolTable::getSymbolUses(&merged.getBodyRegion());
  if (!uses) {
    error() << "the merged module contains an operation whose symbol uses "
            << "cannot be enumerated\n";
    return false;
  }
  for (const SymbolTable::SymbolUse &use : *uses) {
    SymbolRefAttr ref = use.getSymbolRef();
    if (!ref.getNestedReferences().empty()) {
      error() << "nested symbol reference " << ref << " at "
              << locString(use.getUser()->getLoc()) << " is not supported\n";
      ok = false;
      continue;
    }
    if (!table.lookup(ref.getRootReference())) {
      error() << "unresolved reference to @" << ref.getRootReference().getValue()
              << " at " << locString(use.getUser()->getLoc()) << "\n";
      ok = false;
    }
  }
  for (auto global : merged.getOps<cir::GlobalOp>()) {
    auto where = [&] { return locString(global.getLoc()); };
    if (auto guard = global.getStaticLocalGuardAttr())
      if (!isa_and_nonnull<cir::GlobalOp>(table.lookup(guard.getName()))) {
        error() << "the static-local guard @" << guard.getName().getValue()
                << " of @" << global.getSymName()
                << " is not a global of the merged module (" << where()
                << ")\n";
        ok = false;
      }
    if (auto tls = global.getTlsRefsAttr()) {
      if (!isa_and_nonnull<cir::FuncOp>(table.lookup(tls.getWrapperName()))) {
        error() << "the thread_local wrapper @"
                << tls.getWrapperName().getValue() << " of @"
                << global.getSymName()
                << " is not a function of the merged module (" << where()
                << ")\n";
        ok = false;
      }
      // The initializer may be absent: clang references the initializer of
      // an extern thread_local weakly, and the defining translation unit
      // only emits it when the variable needs dynamic initialization.
      if (StringAttr init = tls.getInitName();
          init && !init.getValue().empty() && table.lookup(init) &&
          !isa<cir::FuncOp>(table.lookup(init))) {
        error() << "the thread_local initializer @" << init.getValue()
                << " of @" << global.getSymName() << " is not a function ("
                << where() << ")\n";
        ok = false;
      }
    }
  }
  for (const char *name : {kGlobalCtorsAttr, kGlobalDtorsAttr}) {
    auto array = merged->getAttrOfType<ArrayAttr>(name);
    if (!array)
      continue;
    for (Attribute entry : array) {
      StringAttr fn;
      if (auto ctor = dyn_cast<cir::GlobalCtorAttr>(entry))
        fn = ctor.getName();
      else if (auto dtor = dyn_cast<cir::GlobalDtorAttr>(entry))
        fn = dtor.getName();
      if (!fn) {
        error() << "unexpected entry in '" << name << "': " << entry << "\n";
        ok = false;
        continue;
      }
      if (!isa_and_nonnull<cir::FuncOp>(table.lookup(fn))) {
        error() << "'" << name << "' names @" << fn.getValue()
                << ", which is not a function of the merged module\n";
        ok = false;
      }
    }
  }
  return ok;
}

// Prints the module with its locations: cir2c maps them back to the sources.
std::string printModule(ModuleOp module) {
  std::string text;
  llvm::raw_string_ostream os(text);
  OpPrintingFlags flags;
  flags.enableDebugInfo(/*enable=*/true, /*prettyForm=*/false);
  module->print(os, flags);
  return text;
}

// Parses the printed output again in a fresh context, as a consumer would.
bool reparses(const std::string &text) {
  MLIRContext context;
  registerDialects(context);
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(
      llvm::MemoryBuffer::getMemBuffer(text, "<merged output>"), llvm::SMLoc());
  SourceMgrDiagnosticHandler handler(sourceMgr, &context);
  ParserConfig config(&context, /*verifyAfterParse=*/true);
  return static_cast<bool>(parseSourceFile<ModuleOp>(sourceMgr, config));
}

// Writes `text` to a temporary file next to `path` and renames it into place,
// so a failed run never leaves a partial output behind.
bool writeAtomically(const std::string &path, const std::string &text) {
  SmallString<256> model(path);
  model += ".tmp-%%%%%%";
  int fd;
  SmallString<256> tmpPath;
  if (std::error_code ec = llvm::sys::fs::createUniqueFile(model, fd, tmpPath)) {
    error() << "cannot create a temporary file next to " << path << ": "
            << ec.message() << "\n";
    return false;
  }
  {
    llvm::raw_fd_ostream os(fd, /*shouldClose=*/true);
    os << text;
    os.close();
    if (os.has_error()) {
      error() << "cannot write " << tmpPath << ": " << os.error().message()
              << "\n";
      os.clear_error();
      llvm::sys::fs::remove(tmpPath);
      return false;
    }
  }
  if (std::error_code ec = llvm::sys::fs::rename(tmpPath, path)) {
    error() << "cannot rename " << tmpPath << " to " << path << ": "
            << ec.message() << "\n";
    llvm::sys::fs::remove(tmpPath);
    return false;
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  llvm::InitLLVM init(argc, argv);
  llvm::cl::HideUnrelatedOptions(mergeCategory);
  llvm::cl::ParseCommandLineOptions(
      argc, argv,
      "cir-merge: merge the CIR modules of several translation units\n\n"
      "Conservative: inputs outside the supported subset are rejected with a\n"
      "diagnostic instead of being merged approximately.\n");

  // 1. Parse and verify every input in a context of its own. Give the
  //    anonymous records their names in the merged module, and compare the
  //    named records, before the inputs share a context (see "Record
  //    definitions").
  std::vector<std::unique_ptr<MLIRContext>> ownContexts;
  std::vector<OwningOpRef<ModuleOp>> ownModules;
  std::vector<llvm::SetVector<cir::RecordType>> records;
  bool ok = true;
  for (const std::string &path : inputPaths) {
    auto own = std::make_unique<MLIRContext>();
    registerDialects(*own);
    OwningOpRef<ModuleOp> module = parseInput(path, *own);
    if (!module) {
      ok = false;
      continue;
    }
    records.push_back(collectRecords(*module));
    ownModules.push_back(std::move(module));
    ownContexts.push_back(std::move(own));
  }
  if (!ok)
    return 1;
  std::vector<std::map<std::string, std::string>> anonRenames =
      assignAnonNames(records);
  std::vector<RecordDefs> recordDefs;
  for (const auto &input : records)
    recordDefs.push_back(namedRecordDefs(input));
  if (!checkRecordDefs(inputPaths, recordDefs))
    return 1;

  MLIRContext context;
  registerDialects(context);
  std::vector<Input> inputs;
  for (size_t i = 0; i < inputPaths.size(); ++i) {
    const std::string &path = inputPaths[i];
    OwningOpRef<ModuleOp> module;
    if (anonRenames[i].empty()) {
      module = parseInput(path, context);
    } else {
      if (verbose)
        for (auto &[from, to] : anonRenames[i])
          llvm::errs() << "cir-merge: record '" << from << "' (in " << path
                       << ") renamed to '" << to << "'\n";
      std::optional<std::string> text = renumberAnonRecords(
          *ownModules[i], path, records[i], anonRenames[i]);
      if (text)
        module = parseBuffer(llvm::MemoryBuffer::getMemBufferCopy(*text, path),
                             path, context);
    }
    if (!module) {
      ok = false;
      continue;
    }
    inputs.push_back({path, std::move(module)});
  }
  if (!ok)
    return 1;
  ownModules.clear();
  ownContexts.clear();

  // 2. Give colliding translation-unit-local symbols unique names.
  if (!renameLocalCollisions(inputs))
    return 1;

  // 3. Module compatibility.
  std::optional<SmallVector<NamedAttribute>> moduleAttrs =
      mergeModuleAttributes(inputs, context);

  // 4. Symbol inventory.
  std::vector<Symbol> symbols;
  bool inventoryOk = buildInventory(inputs, symbols);
  if (dumpSymbols)
    printInventory(inputs, symbols);
  if (!moduleAttrs || !inventoryOk)
    return 1;

  // 5. Plan, for the whole inventory, before anything is modified.
  Plan plan;
  if (!planMerge(inputs, symbols, plan))
    return 1;

  // 6. Apply, then validate the result.
  OwningOpRef<ModuleOp> merged = applyPlan(inputs, plan, *moduleAttrs, context);
  if (failed(verify(*merged))) {
    error() << "the merged module does not verify (see above)\n";
    return 1;
  }
  if (!checkReferences(*merged))
    return 1;

  std::string text = printModule(*merged);
  if (!reparses(text)) {
    error() << "the merged module does not parse back (see above)\n";
    return 1;
  }
  return writeAtomically(outputPath, text) ? 0 : 1;
}
