#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void str_copy (void){
    # if 0
    char *temp_1 = "vishnu";
    char *temp_2 = NULL;
    size_t len = strlen(temp_1);
    temp_2 = (char*)malloc(len);
    size_t i ;
    for (i = 0; i <= len; i++)
    {
        temp_2[i] = temp_1[i];
    }
    temp_2[i] = '\0';
    printf("second string: %s\n", temp_2);
    #endif
    #if 0
    char *temp_1 = "vishnu";
    size_t len = strlen(temp_1);
    char temp_2[len];
    size_t i ;
    for (i = 0; i <= len; i++)
    {
        temp_2[i] = temp_1[i];
    }
    temp_2[i] = '\0';
    printf("second string: %s\n", temp_2);
    #endif
    char *temp_1 = "vishnu";
    size_t len = strlen(temp_1);
    char temp_2[len];
    strcpy(temp_2, temp_1);
    printf("second string: %s\n", temp_2);
}
void str_concatenation(void){
    char *temp_1 = "string - 1 ";
    char temp_2[] = "string - 2";
    size_t total_size = strlen(temp_1) + strlen(temp_2) + 1; // plus 1 for the null terminator
    char result[total_size];
    result[0] = '\0'; // Initialize result as an empty string
   // size_t i = 0, j = 0;
#if 0 
    while (i < (strlen(temp_1)))
    {
        result[i] = temp_1[i];
        i++;
    }
    while (i < total_size)
    {
        printf("gg\n");
        result[i] = temp_2[j];
        j++;
        i++;
    }
    result[i] = '\0';
#endif

#if 0 
    for(i = 0 ; i < strlen(temp_1); i++){
        result[i] = temp_1[i]; 
    }
    for(j = 0 ; j < strlen(temp_1); j++, i++){
        result[i] = temp_2[j]; 
    }
    result[i] = '\0';
#endif

    strcat(result, temp_1);
    strcat(result, temp_2);
    printf("concatenated string: %s\n", result);

}
void str_reverse(void){
    char original[] = "esrever";
    size_t i = (strlen(original) - 1);
    size_t j = 0;
    while ( i > j)
    {
        char temp = original[j];
        original[j] = original [i];
        original[i] = temp;
        i--;
        j++;
    }
    printf("Reversed: %s\n", original);

#if 0 
    size_t i = strlen(original);

    // Allocate memory for reversed string (+1 for null terminator)
    char *reverse  = (char *)malloc((sizeof(char) * i) + 1);
    if (reverse == NULL) {
        printf("Memory allocation failed\n");
        return;
    }

    // Reverse the string
    for (size_t j = 0; j < i; j++) {
        reverse[j] = original[i - 1 - j];
    }

    reverse[i] = '\0'; // Null-terminate the reversed string

    printf("Reversed: %s\n", reverse);

    free(reverse); // Free dynamically allocated memory
#endif

}


int main(void){
    //str_copy();
    //str_concatenation();
    str_reverse();
    return 0;
}