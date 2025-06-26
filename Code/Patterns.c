#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>

void right_diamond_pyramid(int n)
{
    int i, j ;
        for (i = 0; i < n; i++)
    {
        // print one less row then other half, because at i = 0 it prints nothing
        for (j = 0; j < i; j++) 
        {
            printf("*");
        }
        printf("\n");
    }
    for (i = 0; i < n; i++)
    {
        // another logic to print the reverse right half pyramid 
        for (j = 0; j < n - i ; j++)
        {
            printf("*");
        }
        
        printf("\n");
    }

    return;
}
void reverse_right_half_pyramid(int n)
{
    for (int i = 0; i < n; i++)
    {
        for (int j = 1; j < n + 1 - i ; j++)
        {
            printf("*");
        }
        printf("\n");
    }
    return;
}
void right_half_pyramid(int n)
{
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j <= i; j++)
        {
            printf("*");
        }
        // Another logic to print the right half pyramid
        /*
        for (int j = 0; j < i + 1; j++)
        {
            printf("*");
        }
        */
        printf("\n");
    }
    return;
}
void left_half_pyramid(int n){
    int i, j, g;
    for(i = 0; i < n; i++){
        // n - 1 -i because i start at 0 and no of empty space must less then 1
        for(j = 0; j < n - 1 - i; j++){
            printf(" ");
        }
        for(g = 0; g <= i; g++){
            printf("*");
        }
        printf("\n");
    }
    return;
}
void reverse_left_half_pyramid(int n){
    int i, j, g;
    for(i = 0; i < n; i++){

        for(j = 0; j < i ; j++){
            printf(" ");
        }
        for(g = 0; g < n - i; g++){
            printf("*");
        }
        printf("\n");
    }
    return;
}
void left_diamond_pyramid(int n){
    int i, j, g;
    for ( i = 0; i < n; i++)
    {
        for(j = 0; j < n - 1 - i; j++){
            printf(" ");
        }
        for (g = 0; g <= i; g++)
        {
            printf("*");
        }
        printf("\n");
    }
    for ( i = 0; i < n - 1; i++)
    {
        for(j = 0; j <= i; j++){
            printf(" ");
        }
        for (g = 0; g < n - 1 - i; g++)
        {
            printf("*");
        }
        printf("\n");
    }
    return;
}

void full_pyramid(int n){
    int i, j, g;
    for (i = 0; i < n; i++)
    {
        for(j = 0; j < n - i - 1; j ++){
            printf(" ");
        }
        for(g = 0; g <= (2*i); g++){
            printf("*");
        }
        printf("\n");

    }
    return;
    
}
void invert_pyramid(int n){
    int i, j;
    for(i = 0; i < n; i++){
        for(j = 0; j < i; j++){
            printf(" ");
        }
        for(j = 0; j < (2*(n - i) - 1); j++){
            printf("*");
        }
        printf("\n");
    }
    return;
}
void diamond_brute(int n){
    int i, j, g;
    for (i = 0; i < n; i++)
    {
        for(j = 0; j < n - i - 1; j ++){
            printf(" ");
        }
        for(g = 0; g <= (2*i); g++){
            printf("*");
        }
        printf("\n");

    }
    for(i = 0; i < n - 1; i++){
        for(j = 0; j <= i; j++){
            printf(" ");
        }
        for(j = 0; j < (2*(n - 1 - i) - 1); j++){
            printf("*");
        }
        printf("\n");
    }
    return;
}
void diamond(int n){
    int i, j;
    int star, space;
    for(i = 0; i < (2*n) - 1; i++){
        if(i < n){
            space = n - i - 1;
            star  = (2*i) + 1;
        }
        else{
            space = i - n + 1;
            star  = (2*(2*n - 1 - i)) - 1;
        }

        for(j = 0; j < space; j++){
            printf(" ");
        }
        for(j = 0; j < star; j++){
            printf("*");
        }
        printf("\n");
    }
}


void right_rhombus(int n){
    int i, j;
    for (i = 0; i < n; i++){
        for(j = 0; j < i; j++){
            printf(" ");
        }
        for(j = 0; j < n; j++){
            printf("*");
        }
        printf("\n");
    }
    return;
}
void left_rhombus(int n){
    int i, j;
    for (i = 0; i < n; i++){
        for(j = 0; j < n - i - 1; j++){
            printf(" ");
        }
        for(j = 0; j < n; j++){
            printf("*");
        }
        printf("\n");
    }
    return;
}

int main(void){
    printf("Enter the pattern number\n");
    int n = 0;
    scanf("%d", &n);
    diamond_brute(n);
    return 0;
}