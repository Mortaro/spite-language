/* The same work tuned by hand: the names and their lengths in arrays on the stack, joined once into a buffer on
 * the stack. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* names[3] = {"ann", "bob", "cy"};
    const int32_t lengths[3] = {3, 3, 2};
    char joined[16];
    int32_t used = 0;
    int32_t longest = 0;
    for (int32_t index = 0; index < 3; index++) {
        if (index > 0) {
            memcpy(joined + used, ", ", 2);
            used += 2;
        }
        memcpy(joined + used, names[index], (size_t)lengths[index]);
        used += lengths[index];
        if (lengths[index] > longest) longest = lengths[index];
    }
    joined[used] = 0;
    printf("%s %d\n", joined, longest);
    return 0;
}
