//--------------------------------------------------------------------------//
// file: queue.h                                                            //
//       --this is a header file for my implementation of a template queue  //
//       class based off list.h                                             //
//                                              Christopher J. Dickey       //
//                                                            2/12/96       //
//--------------------------------------------------------------------------//

#include "queue.h"


//------------------------------------queueClass-----------------------------//

template <class T>
class queueClass : public listClass
{
public:
  queueClass() : head(NULL), tail(NULL), num_items(0)
  {}
  queueClass(const queueClass<T>& Q);
  ~queueClass();

  // Enqueue and dequeue add and remove items from the queue, they do NOT
  // however free up or create the item--that's left up to the coder
  virtual bool AddItem(nodeStruct<T> *node, T* item);
  virtual bool RemoveItem(nodeStruct<T> *node);

protected:
  nodeStruct<T> *tail;
};


//--------------------------queueClass member functions----------------------//
template <class T>
queueClass<T>::queueClass(const queueClass<T>& Q)
{
  num_items = Q.NumItems();

  if (Q.head == NULL)
    head = NULL;
  else {
    // make sure we can allocate a new struct
    assert((head = new nodeStruct<T>) != NULL);

    // and a new data item
    assert((head->data = new T) != NULL);

    // now copy the data over
    head->data = Q.head->data;

    // zoom through the list creating the data properly and allocating memory
    for (nodeStruct<T> *temp = Q.head->
                               next, *newlist = head;
         temp;
         temp = temp->next) {
      assert ((newlist->next = new nodeStruct<T>) != NULL);
      newlist = newlist->next;
      assert ((newlist->data = new T) != NULL);
      newlist->data = temp->data;
    }

    // the last 'next' pointer must = NULL
    newlist->next = NULL;
  }
}

// all I have to do here is set tail to NULL, the actual items get deleted
// when the base class destructors get called
template <class T>
queueClass<T>::~queueClass()
{
  tail = NULL;
}

// in a queue, you add items to the tail, and retrieve them from the head
// but since we are using listClass as a base class, when you add an item
// it goes into the head, so things work kinda backwards
template <class T>
bool queueClass<T>::EnQueue(T *item)
{
  // what I do here is add an item and move the 'tail' pointer up one
  // so I know where to pull items from
  if (head) {
    assert (AddItem(NULL, item));
    tail = tail->next;
  } else {
    assert (AddItem(NULL, item));
    tail = head;
  }
}

// the obvious problem with dequeue is having to move the tail up the list
// one -- one of the many cases for using doubly linked lists
// of course, this is also a great example of why OOP is cool, as I can go
// back in and change the base class which implements how the lists work
// and it won't affect how the program runs
template <class T>
bool queueClass<T>::DeQueue()
{
  // first we remove the item
  RemoveItem(tail);
  // then we zoom through the list and find out where the tail should really
  // be
  register nodeStruct<T> *temp = head;
  while (temp)
    temp = temp->next;

  tail = temp;
}
