#include <stdio.h>
#include <stdlib.h>

struct Node {
    int data;
    struct Node* next;
};
struct Node* createNode(int data) {
    struct Node* newnode = (struct Node*)malloc(sizeof(struct Node));
    if (newnode == NULL) return NULL;
    newnode->data = data;
    newnode->next = NULL;
    return newnode;
}

struct Node* insertAtEnd(struct Node* head_ref, int new_data, int* size) {
    struct Node* new_node = createNode(new_data);
    if (new_node == NULL) {
        printf("Memory allocation failed\n");
        return head_ref;
    }

    (*size)++;  

    if (head_ref == NULL) {
        return new_node;
    }

    struct Node* last = head_ref;
    while (last->next != NULL) {
        last = last->next;
    }

    last->next = new_node;
    return head_ref;
}

void printList(struct Node* node) {
    while (node != NULL) {
        printf("%d -> ", node->data);
        node = node->next;
    }
    printf("NULL\n");
}

void freeList(struct Node* node) {
    struct Node* temp;
    while (node != NULL) {
        temp = node;
        node = node->next;
        free(temp);
    }
}

void insertrandom(int i, int data, struct Node** head, int* size) {
    struct Node* newnode = createNode(data);
    if (newnode == NULL) {
        printf("Memory allocation failed\n");
        return;
    }

    if (i <= 0 || i > (*size) + 1) {
        printf("Invalid position\n");
        free(newnode);
        return;
    }

    (*size)++;

    if (i == 1) {
        newnode->next = *head;
        *head = newnode;
        return;
    }

    struct Node* current = *head;
    for (int j = 1; j < i - 1 && current != NULL; j++) {
        current = current->next;
    }

    if (current == NULL) {
        printf("Position is out of bounds\n");
        free(newnode);
        return;
    }

    newnode->next = current->next;
    current->next = newnode;
}

void Bubblesorting(struct Node* head, int size) {
    struct Node* current;
    struct Node* next;
    int temp;

    if (head == NULL || head->next == NULL) return;

    for (int i = 0; i < size - 1; i++) {
        current = head;
        next = current->next;
        int swaps = 0;

        for (int j = 0; j < size - i - 1; j++) {
            if (current->data > next->data) {
                temp = current->data;
                current->data = next->data;
                next->data = temp;
                swaps = 1;
            }
            current = next;
            next = next->next;
        }

        if (swaps == 0) break;
    }
}

void selection_sort(struct Node* head, int size){
    struct Node* current;
    struct Node* next;
    int temp;

    if (head == NULL || head->next == NULL) return;

    for (int i = 0; i < size - 1; i++) {
        current = head;
        next = current->next;

        for (int j = i + 1; j < size; j++) {
            if (next->data < current->data) {
                temp = current->data;
                current->data = next->data;
                next->data = temp;
            }
            next = next->next;
        }
        head = head->next;
    }
}

int main(void) {
    struct Node* head = NULL;
    int* size = (int*)malloc(sizeof(int));
    *size = 0;

    head = insertAtEnd(head, 1, size);
    head = insertAtEnd(head, 77, size);
    head = insertAtEnd(head, 3, size);

    printf("Created Linked List:\n");
    printList(head);

    int i = 0, data = 0;
    printf("Enter the position and data to insert:\n");
    scanf("%d%d", &i, &data);

    insertrandom(i, data, &head, size);

    printf("List after random insertion:\n");
    printList(head);

    Bubblesorting(head, *size);
    printf("Bubble Sorted List:\n");
    printList(head);

    Bubblesorting(head, *size);
    printf("Bubble Sorted List:\n");
    printList(head);
    
    freeList(head);
    free(size);
    return 0;
}


/*int main(void) {
    struct Node* head =NULL;
    struct Node* second = NULL;
    struct Node* third = NULL;
    // Allocate nodes in the heap
    head = (struct Node*)malloc(sizeof(struct Node));
    second = (struct Node*)malloc(sizeof(struct Node));
    third = (struct Node*)malloc(sizeof(struct Node));
    head->data = 1; // Assign data in first node
    head->next = second; // Link first node with second
    second->data = 2; // Assign data to second node
    second->next = third; // Link second node with third
    third->data = 3; // Assign data to third node
    third->next = NULL; // Terminate the list at third node
    // Print the linked list
    struct Node* Current = NULL;
    Current = head;
    while (Current != NULL ){
        int i = 0;
        printf("Current Node:%d and Data in the Node:%d\n ", i, Current->data);
        Current = Current->next;
        i++;
    }
    free(head); // Free the allocated memory
    free(second);
    free(third);
    return 0;
}
*/