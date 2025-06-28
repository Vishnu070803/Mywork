#include <math.h>
#include <stdbool.h>
#include <stdio.h>

int main() {
    int n = 29;
    int cnt = 0;

    // If number is less than/equal to 1,
    // it is not prime
    if (n <= 1)
        printf("%d is NOT prime", n);
    else {

        // Check how many numbers divide n in
        // range 2 to sqrt(n)
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