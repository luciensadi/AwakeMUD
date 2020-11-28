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
using namespace std;

#define ADD(a)  Add(a, __FILE__, __LINE__)

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE  (!FALSE)
#endif


template <class T>
struct nodeStruct
{
  T data;
  nodeStruct<T> *next;
};

//==========================listClass class===============================//

template <class T>
class listClass
{
public:
  // constructors and destructors
  listClass() : num_items(0), head(NULL)
  {}
  listClass(const listClass<T>& L);
  virtual ~listClass();

  int NumItems()
  {
    return num_items;
  }
  // AddItem adds an item after the node
  bool AddItem(nodeStruct<T> *node, T item);
  // RemoveItem the item in the list and removes it, however, a
  // function to find the item should be defined in a child class
  bool RemoveItem(nodeStruct<T> *node);
  nodeStruct<T> *FindItem(T item);
  nodeStruct<T> *Head()
  {
    return head;
  }


private:
  int num_items;

protected:
  nodeStruct<T> *head;
};

//-------------------------------------------------------------------------//
// member functions for the classes follow                                 //
//-------------------------------------------------------------------------//

template <class T>
listClass<T>::listClass(const listClass<T> & L)
{
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

    // copy the data over
    head->data = L.head->data;

    // now we loop through the list and allocate and create the new one
    nodeStruct<T> *temp, *newlist;
    for (temp = L.head->next, newlist = head; temp;
         temp = temp->next) {
      assert((newlist->next = new nodeStruct<T>) != NULL);
      newlist = newlist->next;
      newlist->data = temp->data;
    }

    // finally, make the last 'next' point to NULL to indicate end of list
    newlist->next = NULL;
  } // end else
}

template <class T>
listClass<T>::~listClass()
{
  // to delete, we just remove and deallocate each item
  register nodeStruct<T> *temp = head;
  while (temp) {
    head = head->next;
    delete temp;
    temp = head;
  }
}

/* this assumes that even if item is a pointer to some type, it's been
 allocated already */
template <class T>
bool listClass<T>::AddItem(nodeStruct<T> *node, T item)
{
  /* AddItem adds the item AFTER the node, if it's NULL it goes at the head */
  nodeStruct<T> *NewNode = new nodeStruct<T>;
  assert(NewNode != NULL);

  if (node == NULL) {
    /* set the data to the parameter, then set the pointers straight and
     pop it onto the head of the list */
    node = NewNode;
    node->data = item;
    node->next = head;
    head = node;
  } else {
    NewNode->next = node->next;
    NewNode->data = item;
    node->next = NewNode;
  }

  num_items++;  // increment num_items since we added

  return TRUE;
}

template <class T>
bool listClass<T>::RemoveItem(nodeStruct<T> *node)
{
  // Remove item assumes you have alread found the node, so child classes
  // will have to define their own rules to find items based on data type
  nodeStruct<T> *found;
  // if its at the head of the list, removal is easy
  if (node == head) {
    // deallocate any memory
    found = head;
    head = found->next;
    delete found;
    num_items--;  /* decrement number of items in list */
    return TRUE;  /* it's been found, so return TRUE */
  } else {
    register nodeStruct<T> *temp = head;
    while (temp && (temp->next != node))
      temp = temp->next;

    if (temp->next == node) { // ie, it was found
      found = temp->next;
      temp->next = found->next;
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
nodeStruct<T> *listClass<T>::FindItem(T item)
{
  register nodeStruct<T> *temp;

  for (temp = head; temp; temp = temp->next)
    if (temp->data == item)
      return temp;

  return NULL;
}

//==============================List class===============================//


// List is a special list used for the mud which checks for duplicate items
// if DEBUG is defined

template <class T>
class List : public listClass<T>
{
public:
  List()
  {}
  List(const List<T>& L)
  {}
  virtual ~List()
  {}

  bool Add(T item, const char *filename, int lineno);
  bool Remove(T item);
  // Traverse will travel the list calling func with the data of each node
  //    void Traverse(void (*func)(T));
};


//============================List member functions=====================//
template <class T>
bool List<T>::Add(T item, const char *filename, int lineno)
{

#ifdef DEBUG
  if (FindItem(item)) {
    cerr << "SYSERR: Attempt to add duplicate item to list in file " << filename
    << ", line: " << lineno << endl;
    abort();
  }
#endif

  return AddItem(NULL, item);
}

template <class T>
bool List<T>::Remove(T item)
{
  return RemoveItem(FindItem(item));
}

#endif
