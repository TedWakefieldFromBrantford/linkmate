#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    printf("Linkmate v0.1.0. - Let's rip Linux a new one, Bitches\n");


    if (argc > 1 ) {
        printf("\U0001F4C1 Reading from file: %s\n", argv[1]);
        // todo: read and extract files
    } else {
        printf("\U0001f92f Reading from yer termimal.\n");
        printf("As in url | ./linkmate.  How do you like dem nutz?\n");
    }

    printf("\U0001f91e  Real scanner coming soon.  Simmer down\n");
    return 0;
}
   
