#include <stdlib.h>

typedef struct Node {
    void* data;
    struct Node* next;
} node_t;

typedef struct {
	node_t* head;
	node_t* tail;
	int size;
} list_t;

list_t* createList() {
	list_t* newList = (list_t*)malloc(sizeof(list_t));
	newList->head = NULL;
	newList->tail = NULL;
	newList->size = 0;
	return newList;
}

node_t* createNode(void* data, size_t data_size) {
	node_t* newNode = (node_t*)malloc(sizeof(node_t));
	newNode->data = malloc(data_size);
	memcpy(newNode->data, data, data_size);
	return newNode;
}

// Adds a node to the end
void add(list_t* list, node_t* node) {
	if(list->head == NULL) {
		list->head = node;
		list->tail = node;
	} else {
		(list->tail)->next = node;
		list->tail = node;
	}
	list->size++;
}

// Pops a node from the head
void popHead(list_t* list) {
	if(list->head != NULL)
	{
		node_t* temp = list->head;
		list->head = (list->head)->next;
		if(list->head == NULL)
			list->tail = NULL;
		list->size--;
		free(temp->data);
		free(temp);
	}
}

// Returns 1 if the list is empty
int isListEmpty(list_t* list) {
	return list->size == 0 ? 1 : 0;
}
