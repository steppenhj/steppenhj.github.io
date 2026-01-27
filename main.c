#include <stdio.h>
int main(int argc, char *argv[]){
    int a=15, b=3, c=12;
    int t1, t2, t3;
    t1 = a && b;
    t2 = a || b;
    t3 = !c;
    printf("%d %d %d %d", t1, t2, t3, t1+t2+t3);
    return 0;
}