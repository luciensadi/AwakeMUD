/**************************************************************************/
/*  file: memory.h  -  1/21/96 -- Christopher J. Dickey                   */
/*    This is a set of classes to handle memory for the mud.  There's     */
/*    not a whole lot of fancy stuff behind them, but they do help in     */
/*    efficiency.  Basically, we never really delete any memory unless    */
/*    absolutely necessary, and we keep stacks of the most used           */
/*    objects.                                                            */
/**************************************************************************/

#ifndef _memory_h_
#define _memory_h_

#define INITIAL_STACK_SIZE      500
#define STACK_SIZE_INCREASE 100

#include "structs.h"
#include "utils.h"

template <class T>
class stackClass
{
public:
  // constructors and destructors
  stackClass();
  stackClass(const stackClass<T>& O);
  ~stackClass();

  // stack routines
  int Size()
  {
    return (top + 1);
  }
  int MaxSize()
  {
    return max_size;
  }
  void Push(T *NewItem);
  T *Pop();
  void PopDelete();
  bool StackIsEmpty()
  {
    return (top < 0 ? 1 : 0);
  }

private:
  // private data for the stack
  int top;
  int max_size;
  // Precondition:  objStackClass must be created already
  // Postcondition: the stack will be increased by the constant
  //    OBJ_STACK_SIZE_INCREASE.  If it is increased, TRUE is returned
  //    else FALSE.
  bool ResizeStack();

  // This is the array of objects
  T **Items;

}
; // end class

template <class T>
stackClass<T>::stackClass()
{
  max_size = INITIAL_STACK_SIZE;
  Items = new T*[max_size];
  top = -1;
}

template <class T>
stackClass<T>::stackClass(const stackClass<T>& O)
{
  max_size = O.max_size;
  top = O.top;

  for (int Index = 0; Index <= O.top; ++Index)
    Items[Index] = O.Items[Index];
}

template <class T>
stackClass<T>::~stackClass()
{
  while (top >= 0)
    PopDelete();
}

template <class T>
void stackClass<T>::Push(T *NewItem)
{
  // make sure you have room in the array
  if (top < max_size - 1) {
    ++top;
    Items[top] = NewItem;
  } else { // if not, resize the array (larger), and re-Push the item
    assert(ResizeStack());
    Push(NewItem);
  }
}

template <class T>
T *stackClass<T>::Pop()
{
  if (top < 0)
    return NULL;

  --top;
  return (Items[top + 1]);
}

template <class T>
void stackClass<T>::PopDelete()
{
  if (top < 0)
    return;

  --top;

  // we just call delete on the object since we know it is insides
  // have already been cleared up when it was put on the stack
  delete (Items[top + 1]);
}

template <class T>
bool stackClass<T>::ResizeStack()
{
  // create a new stack array larger than the previous
  T **NewItems;

  NewItems = new T*[max_size + STACK_SIZE_INCREASE];

  // now copy the old stack array into the new one
  for (int Index = 0; Index <= top; ++Index)
    NewItems[Index] = Items[Index];

  // delete the elements of the old stack
  // what were you thinking, Chris?  DO NOT delete the elements of the stack,
  // as we just passed the pointers to them above!  Just get rid of the array
  // itself
  //  delete [] *Items;
  delete Items;
  // point Items to the new stack now
  Items = NewItems;

  max_size += STACK_SIZE_INCREASE;

  mudlog("Resizing Stack...", NULL, LOG_SYSLOG, TRUE);
  return TRUE;
}

class memoryClass
{
private:
  stackClass<struct obj_data> *Obj;
  stackClass<struct char_data> *Ch;
  stackClass<struct room_data> *Room;
  stackClass<struct veh_data> *Veh;
  stackClass<struct host_data> *Host;
  stackClass<struct matrix_icon> *Icon;

public:
  // constructors and destructors
  memoryClass();
  ~memoryClass();
  memoryClass(const memoryClass& mClass);

  // size operations for general info
  int ObjSize()
  {
    return Obj->Size();
  }
  int ChSize()
  {
    return Ch->Size();
  }
  int RoomSize()
  {
    return Room->Size();
  }
  int VehSize()
  {
    return Veh->Size();
  }
  int HostSize()
  {
    return Host->Size();
  }
  int IconSize()
  {
    return Icon->Size();
  }
  int ObjMaxSize()
  {
    return Obj->MaxSize();
  }
  int ChMaxSize()
  {
    return Ch->MaxSize();
  }
  int RoomMaxSize()
  {
    return Room->MaxSize();
  }
  int VehMaxSize()
  {
    return Veh->MaxSize();
  }
  int HostMaxSize()
  {
    return Host->MaxSize();
  }
  int IconMaxSize()
  {
    return Icon->MaxSize();
  }

  // get routines which return objects from the different stacks
  struct obj_data *GetObject();
  struct char_data *GetCh();
  struct room_data *GetRoom();
  struct veh_data  *GetVehicle();
  struct host_data *GetHost();
  struct matrix_icon *GetIcon();

  // delete routines which push objects onto the stacks after deallocating
  // all strings and such in the object
  void DeleteObject(struct obj_data *obj);
  void DeleteCh(struct char_data *ch);
  void DeleteRoom(struct room_data *room);
  void DeleteVehicle(struct veh_data *veh);
  void DeleteHost(struct host_data *host);
  void DeleteIcon(struct matrix_icon * icon);

  // clear routines which push objects onto the stacks after just clearing
  // the variables and pointers in the objects.  These will not wipe out
  // the strings and other allocated vars
  void ClearObject(struct obj_data *obj);
  void ClearCh(struct char_data *ch);
  void ClearRoom(struct room_data *room);
  void ClearVehicle(struct veh_data *veh);
  void ClearHost(struct host_data *host);
  void ClearIcon(struct matrix_icon *icon);

  // clear stacks
  bool ClearStacks();
};

extern class memoryClass *Mem;
extern class objList ObjList;

#endif
