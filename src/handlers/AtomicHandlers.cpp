/*
 * Copyright 2025 Budapest University of Technology and Economics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HandlerModule.h"
#include "Mapper.h"

#include <mlir/IR/Operation.h>
#include <llvm/Support/Casting.h>
#include <clang/CIR/Dialect/IR/CIRDialect.h>

#include <string>

using namespace mlir;

namespace cir2c {

/// Handlers for the read-modify-write atomics (cir.atomic.*), mapped to the
/// GNU __atomic_* builtins. Atomic loads and stores are not here: they are
/// cir.load / cir.store with a mem_order, see MemoryHandlers.
class AtomicHandlers : public HandlerModule {
public:
  void registerHandlers(Mapper &m) override {
    m.registerTypedHandler<cir::AtomicFetchOp>(handleFetch);
    m.registerTypedHandler<cir::AtomicXchgOp>(handleXchg);
    m.registerTypedHandler<cir::AtomicCmpXchgOp>(handleCmpXchg);
    m.registerTypedHandler<cir::AtomicFenceOp>(handleFence);
    m.registerTypedHandler<cir::AtomicClearOp>(handleClear);
    m.registerTypedHandler<cir::AtomicTestAndSetOp>(handleTestAndSet);
  }

private:
  static std::string order(cir::MemOrder o) {
    return std::to_string(static_cast<uint32_t>(o));
  }

  // A direct-access value is named by its lvalue, not by a pointer to it.
  static std::string addr(mlir::Value ptr, Mapper &m) {
    std::string name = m.getOrCreateName(ptr);
    return m.isDirectAccess(ptr) ? "&(" + name + ")" : name;
  }

  static bool handleFetch(cir::AtomicFetchOp op, Mapper &m, std::ostream &out) {
    const char *name = nullptr;
    switch (op.getBinop()) {
    case cir::AtomicFetchKind::Add:  name = "add"; break;
    case cir::AtomicFetchKind::Sub:  name = "sub"; break;
    case cir::AtomicFetchKind::And:  name = "and"; break;
    case cir::AtomicFetchKind::Xor:  name = "xor"; break;
    case cir::AtomicFetchKind::Or:   name = "or"; break;
    case cir::AtomicFetchKind::Nand: name = "nand"; break;
    default: return false; // min/max and friends have no __atomic_* builtin
    }
    std::string builtin = op.getFetchFirst() ? std::string("__atomic_fetch_") + name
                                             : std::string("__atomic_") + name + "_fetch";
    std::string tmp = m.freshName("atomic");
    out << "  " << m.mapTypeToC(op.getResult().getType()) << " " << tmp << " = " << builtin << "("
        << addr(op.getPtr(), m) << ", " << m.getOrCreateName(op.getVal()) << ", "
        << order(op.getMemOrder()) << ");\n";
    m.setName(op.getResult(), tmp);
    return true;
  }

  static bool handleXchg(cir::AtomicXchgOp op, Mapper &m, std::ostream &out) {
    std::string tmp = m.freshName("atomic");
    out << "  " << m.mapTypeToC(op.getResult().getType()) << " " << tmp
        << " = __atomic_exchange_n(" << addr(op.getPtr(), m) << ", "
        << m.getOrCreateName(op.getVal()) << ", " << order(op.getMemOrder()) << ");\n";
    m.setName(op.getResult(), tmp);
    return true;
  }

  static bool handleCmpXchg(cir::AtomicCmpXchgOp op, Mapper &m, std::ostream &out) {
    // The builtin writes the value it saw through `expected`; CIR has it as a
    // result instead.
    std::string old = m.freshName("atomic");
    std::string success = m.freshName("atomic");
    out << "  " << m.mapTypeToC(op.getOld().getType()) << " " << old << " = "
        << m.getOrCreateName(op.getExpected()) << ";\n";
    out << "  _Bool " << success << " = __atomic_compare_exchange_n("
        << addr(op.getPtr(), m) << ", &" << old << ", "
        << m.getOrCreateName(op.getDesired()) << ", " << (op.getWeak() ? 1 : 0) << ", "
        << order(op.getSuccOrder()) << ", " << order(op.getFailOrder()) << ");\n";
    m.setName(op.getOld(), old);
    m.setName(op.getSuccess(), success);
    return true;
  }

  static bool handleFence(cir::AtomicFenceOp op, Mapper &m, std::ostream &out) {
    auto scope = op.getSyncscope();
    bool signal = scope && *scope == cir::SyncScopeKind::SingleThread;
    out << "  " << (signal ? "__atomic_signal_fence(" : "__atomic_thread_fence(")
        << order(op.getOrdering()) << ");\n";
    return true;
  }

  static bool handleClear(cir::AtomicClearOp op, Mapper &m, std::ostream &out) {
    out << "  __atomic_clear(" << addr(op.getPtr(), m) << ", "
        << order(op.getMemOrder()) << ");\n";
    return true;
  }

  static bool handleTestAndSet(cir::AtomicTestAndSetOp op, Mapper &m, std::ostream &out) {
    std::string tmp = m.freshName("atomic");
    out << "  _Bool " << tmp << " = __atomic_test_and_set(" << addr(op.getPtr(), m)
        << ", " << order(op.getMemOrder()) << ");\n";
    m.setName(op.getResult(), tmp);
    return true;
  }
};

REGISTER_HANDLER_MODULE(AtomicHandlers)

} // namespace cir2c
