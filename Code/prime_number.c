#include <math.h>
#include <stdbool.h>
#include <stdio.h>

int main1() {
    int n = 29;
    int cnt = 0;

    if (n <= 1)
        printf("%d is NOT prime", n);
    else {
        for (int i = 2; i * i <= n; i++) {
            if (n % i == 0)
                cnt++;
        }

        // if cnt is greater than 0 then n is
        // not prime
        if (cnt > 0)
            printf("%d is NOT prime", n);

        // else n is prime
        else
            printf("%d is prime", n);
    }
    return 0;
}
int main() {
    int n = 101;
    int cnt = 0;

    if (n <= 1)
        printf("%d is NOT prime", n);
    else {
        for (int i = 2; i <= n / 2; i++) {
            if (n % i == 0)
                cnt++;
        }

        // if cnt is greater than 0 then n is
        // not prime
        if (cnt > 0)
            printf("%d is NOT prime", n);

        // else n is prime
        else
            printf("%d is prime", n);
    }
    return 0;
}