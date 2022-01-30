/* Holy shit this file
 * Is fucking ugly!
 * What the fuck was this
 * guy thinking?!  Like --
 * indent man indent!!!
 * remind me to clean this
 * shit up!  --Dunk */

/*
 ----------------------------------------------------------------------//
 file: list.h                                                         //
 purpose: defines a node structure and a linked list template class   //
 author: Chris Dickey - 2/1/96                                        //
                                                                      //
----------------------------------------------------------------------//
*/
#ifndef _inlist_h_
#define _inlist_h_

#include <assert.h>
#include <iostream>
//using namespace std;

#define ADD(a)  Add(a, __FILE__, __LINE__)

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE  (!FALSE)
#endif


template <class T>
struct nodeStruct {
  T data;
  nodeStruct<T> *next;
  nodeStruct<T> *prev;
  
  nodeStruct() :
    next(NULL), prev(NULL)
  {}
};

//==========================listClass class===============================//

template <class T>
class listClass {
private:
  int num_items;
  
protected:
  nodeStruct<T> *head;
  nodeStruct<T> *tail;
  
public:
  // Constructor: Make an empty list.
  listClass() :
    num_items(0), head(NULL), tail(NULL)
  {}
  
  // Constructor: Clone an existing list.
  listClass(const listClass<T>& L);
  
  // Destructor.
  virtual ~listClass();

  // Operator: Copy assignnment default
  listClass& operator=(const listClass & L) = default;
  
  // AddItem adds an item after the specified node (if none, add at head).
  bool AddItem(nodeStruct<T> *node, T item);
  
  // RemoveItem the item in the list and removes it, however, a
  // function to find the item should be defined in a child class
  bool RemoveItem(nodeStruct<T> *node);
  
  // What it says on the tin.
  nodeStruct<T> *FindItem(T item);
  
  // Simple item retrieval.
  int NumItems() { return num_items; }
  nodeStruct<T> *Head() { return head; }
  nodeStruct<T> *Tail() { return tail; }
};

//-------------------------------------------------------------------------//
// member functions for the classes follow                                 //
//-------------------------------------------------------------------------//

// Clone an existing list.
template <class T>
listClass<T>::listClass(const listClass<T> & L) {
  num_items = L.num_items;

  /* if the head of the old list is empty (NULL), then just set the
   new head to NULL and we're done */

  if (L.head == NULL)
    head = NULL;
  // if not, we have to run through the list and allocate needed memory
  else {
    /* we'll just exit if we can't allocate a nodeStruct, and we can improve
    if we need to manage our own memory */
    assert((head = new nodeStruct<T>) != NULL);

    // shallow copy head's data over (don't clone pointers etc-- just use the same ones as the original)
    head->data = L.head->data;
    
    // Set up head->prev to be null.
    head->prev = NULL;

    // now we loop through the list and allocate and create the new one
    nodeStruct<T> *currnode = head;
    for (nodeStruct<T> *temp = L.head->next; temp; temp = temp->next) {
      // Create the new next node.
      assert((currnode->next = new nodeStruct<T>) != NULL);
      
      // Link it to our current node.
      currnode->prev = currnode;
      
      // Switch operations to the newly-created next node.
      currnode = currnode->next;
      
      // shallow copy this node's data over
      currnode->data = temp->data;
    }

    // finally, make the last 'next' point to NULL to indicate end of list
    currnode->next = NULL;
    tail = currnode;
  } // end else
}

template <class T>
listClass<T>::~listClass() {
  // to delete, we just remove and deallocate each item
  nodeStruct<T> *temp = head;
  while (temp) {
    head = head->next;
    delete temp;
    temp = head;
  }
}

/* this assumes that even if item is a pointer to some type, it's been
 allocated already */
template <class T>
bool listClass<T>::AddItem(nodeStruct<T> *node, T item) {
  assert(item != NULL);
  
  /* AddItem adds the item AFTER the node, if it's NULL it goes at the head */
  nodeStruct<T> *NewNode = new nodeStruct<T>;
  assert(NewNode != NULL);
  
  // Shallow copy the item into our new node.
  NewNode->data = item;

  if (node == NULL) {
    // No position specified means we add to the head of our list.
    NewNode->next = head;
    NewNode->prev = NULL;
    head = NewNode;
  } else {
    // Position specified means we add our new node immediately after the specified one.
    NewNode->next = node->next;
    NewNode->prev = node;
    node->next = NewNode;
  }
  
  // Tie in the reverse link of the next item in the list, provided it exists.
  if (NewNode->next)
    NewNode->next->prev = NewNode;
  
  // Tail check-- if we're adding after last item (or last item is null), update tail pointer.
  if (tail == node || tail == NULL)
    tail = NewNode;

  num_items++;  // increment num_items since we added

  return TRUE;
}

template <class T>
bool listClass<T>::RemoveItem(nodeStruct<T> *node) {
  assert(node != NULL);
  // Remove item assumes you have alread found the node, so child classes
  // will have to define their own rules to find items based on data type
  nodeStruct<T> *found;
  // if its at the head of the list, removal is easy
  if (node == head) {
    // deallocate any memory
    found = head;
    head = found->next;
    
    // If it's a one-item list, clean up the tail.
    if (tail == node)
      tail = NULL;
    
    delete found;
    num_items--;  /* decrement number of items in list */
    return TRUE;  /* it's been found, so return TRUE */
  } else {
    nodeStruct<T> *temp = head;
    while (temp && (temp->next != node))
      temp = temp->next;

    if (temp && temp->next == node) { // ie, it was found
      found = temp->next;
      temp->next = found->next;
      
      // Tail check-- if it's the last item, clean up the tail.
      if (tail == found)
        tail = temp;
      
      if (found)
        delete found;
      num_items--;  // decrement number of items in list
      return TRUE;  // yay, we found it so we return TRUE
    }
  }
  // Could not find it, oddly enough, so we return false.
  return FALSE;
}

// this function runs through the list and compares data, returning the node
// if it is found
template <class T>
nodeStruct<T> *listClass<T>::FindItem(T item) {
  nodeStruct<T> *temp;

  for (temp = head; temp; temp = temp->next)
    if (temp->data == item)
      return temp;

  return NULL;
}

//==============================List class===============================//


// List is a special list used for the mud which checks for duplicate items
// if DEBUG is defined

template <class T>
class List : public listClass<T> {
public:
  List() {}
  List(const List<T>& L) {}
  virtual ~List() {}

  bool Add(T item, const char *filename, int lineno);
  bool Remove(T item);
  // Traverse will travel the list calling func with the data of each node
  //    void Traverse(void (*func)(T));
};


//============================List member functions=====================//
template <class T>
bool List<T>::Add(T item, const char *filename, int lineno) {

#ifdef DEBUG
  if (item == NULL) {
    std::cerr << "SYSERR: Attempt to add null to list in file " << filename << ", line: " << lineno << std::endl;
  }
  
  if (this->FindItem(item)) {
    std::cerr << "SYSERR: Attempt to add duplicate item to list in file " << filename << ", line: " << lineno << std::endl;
    abort();
  }
#endif

  return this->AddItem(NULL, item);
}

template <class T>
bool List<T>::Remove(T item) {
  return this->RemoveItem(this->FindItem(item));
}

#endif
