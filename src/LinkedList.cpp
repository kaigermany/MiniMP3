
#include "LinkedList.h"


LinkedList::LinkedList() : size(0),firstEntry(nullptr),lastEntry(nullptr) {}
LinkedList::~LinkedList(){
	clear();
}

void LinkedList::clear(){
	LinkedListEntry* next = firstEntry;
	firstEntry = 0;
	lastEntry = 0;
	while(next){
		LinkedListEntry* temp = next->next;
		free(next);
		next = temp;
	}
}

void LinkedList::removeEntry(void* obj){
	LinkedListEntry* curr = firstEntry;
	if(!curr) return;
	
	if(curr->object == obj){//if isFirstEntry
		//char isLastEntry = curr == lastEntry;
		firstEntry = curr->next;
		
		if(/*isLastEntry*/curr == lastEntry){//if its also the lat entry
			lastEntry = firstEntry;//schould be NULL.
		}
		
		free(curr);
		size--;
	} else {
		while(1){
			LinkedListEntry* next = curr->next;
			if(!next) return;
			if(next->object == obj){
				
				if(next == lastEntry){
					lastEntry = curr;
				}
				
				curr->next = next->next;
				free(next);
				size--;
				return;
			}
			curr = next;
		}
	}
}

bool LinkedList::addEntry(void* obj){
	LinkedListEntry* curr = (LinkedListEntry*)malloc(sizeof(LinkedListEntry));
	if(!curr) return 1;
	curr->object = obj;
	curr->next = 0;
	if(size == 0){
		firstEntry = lastEntry = curr;
	} else {
		//if(!list->lastEntry) Serial.println("ERROR: list->lastEntry CANNOT BE NULL HERE!");
		lastEntry->next = curr;
		lastEntry = curr;
	}
	size++;
	return 0;
}

void* LinkedList::removeFirstEntry(){
	if(size == 0) return 0;
	LinkedListEntry* out = firstEntry;
	size--;
	if(size == 0) {
		firstEntry = 0;
		lastEntry = 0;
	} else {
		//if(!list->firstEntry) Serial.println("ERROR: list->firstEntry CANNOT BE NULL HERE!");
		firstEntry = firstEntry->next;
	}
	if(!out) return 0;
	//if(!out) Serial.println("ERROR: out CANNOT BE NULL HERE!");
	void* data = out->object;
	free(out);
	return data;
}

void* LinkedList::getEntry(int index){
	LinkedListEntry* curr = firstEntry;
	if(!curr) return 0;
	if(index == 0) return curr->object;
	int currPos = 0;
	while(1){
		currPos++;
		LinkedListEntry* next = curr->next;
		if(!next) return 0;
		if(currPos == index){
			return next->object;
		}
		curr = next;
	}
}
