/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 * Copyright 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define PRINTF(fmt, ...)
//#define PRINTF printf

#include "container.hh"
#include "misc.hh"
#include "debug.hh"

namespace lout {

using namespace object;

namespace container {

namespace untyped {

// -------------
//    Iterator
// -------------

Iterator::Iterator()
{
   impl = NULL;
}

Iterator::Iterator(const Iterator &it2)
{
   impl = it2.impl;
   if (impl)
      impl->ref();
}

Iterator::Iterator(Iterator &it2)
{
   impl = it2.impl;
   if (impl)
      impl->ref();
}

Iterator &Iterator::operator=(const Iterator &it2)
{
   if (impl)
      impl->unref();
   impl = it2.impl;
   if (impl)
      impl->ref();
   return *this;
}

Iterator &Iterator::operator=(Iterator &it2)
{
   if (impl)
      impl->unref();
   impl = it2.impl;
   if (impl)
      impl->ref();
   return *this;
}

Iterator::~Iterator()
{
   if (impl)
      impl->unref();
}

// ----------------
//    Collection
// ----------------

void Collection::intoStringBuffer(misc::StringBuffer *sb)
{
   sb->append("{ ");
   bool first = true;
   for (Iterator it = iterator(); it.hasNext(); ) {
      if (!first)
         sb->append(", ");
      it.getNext()->intoStringBuffer(sb);
      first = false;
   }
   sb->append(" }");
}

// ------------
//    Vector
// ------------

Vector::Vector(int initSize, bool ownerOfObjects)
{
   DBG_OBJ_CREATE ("lout::container::untyped::Vector");

   numAlloc = initSize == 0 ? 1 : initSize;
   this->ownerOfObjects = ownerOfObjects;
   numElements = 0;
   array = (Object**)malloc(numAlloc * sizeof(Object*));
}

Vector::~Vector()
{
   clear();
   free(array);

   DBG_OBJ_DELETE ();
}

int Vector::size ()
{
   return numElements;
}

void Vector::put(Object *newElement, int newPos)
{
   if (newPos == -1)
      newPos = numElements;

   // Old entry is overwritten.
   if (newPos < numElements) {
      if (ownerOfObjects && array[newPos])
         delete array[newPos];
   }

   // Allocated memory has to be increased.
   if (newPos >= numAlloc) {
      while (newPos >= numAlloc)
         numAlloc *= 2;
      void *vp = realloc(array, numAlloc * sizeof(Object*));
      if (vp == NULL) {
         perror("realloc failed");
         exit(1);
      }
      array = (Object**)vp;
   }

   // Insert NULL's into possible gap before new position.
   for (int i = numElements; i < newPos; i++)
      array[i] = NULL;

   if (newPos >= numElements)
      numElements = newPos + 1;

   array[newPos] = newElement;
}

void Vector::clear()
{
   if (ownerOfObjects) {
      for (int i = 0; i < numElements; i++)
         if (array[i])
            delete array[i];
   }

   numElements = 0;
}

void Vector::insert(Object *newElement, int pos)
{
   if (pos >= numElements)
      put(newElement, pos);
   else {
      numElements++;

      if (numElements >= numAlloc) {
         // Allocated memory has to be increased.
         numAlloc *= 2;
         void *vp = realloc(array, numAlloc * sizeof(Object*));
         if (vp == NULL) {
            perror("realloc failed");
            exit(1);
         }
         array = (Object**)vp;
      }

      for (int i = numElements - 1; i > pos; i--)
         array[i] = array[i - 1];

      array[pos] = newElement;
   }
}

void Vector::remove(int pos)
{
   if (ownerOfObjects && array[pos])
      delete array[pos];

   for (int i = pos + 1; i < numElements; i++)
      array[i - 1] = array[i];

   numElements--;
}

/**
 * Sort the elements in the vector. Assumes that all elements are Comparable's.
 */
void Vector::sort(Comparator *comparator)
{
   Comparator::compareFunComparator = comparator;
   qsort (array, numElements, sizeof(Object*), Comparator::compareFun);
}

/**
 * \brief Use binary search to find an element in a sorted vector.
 *
 * If "mustExist" is true, only exact matches are found; otherwise, -1
 * is returned. If it is false, the position of the next greater
 * element is returned, or, if the key is the greatest element, the
 * size of the array. (This is the value which can be used for
 * insertion; see insertSortet()).
 */
int Vector::bsearch(Object *key, bool mustExist, int start, int end,
                    Comparator *comparator)
{
   // The case !mustExist is not handled by bsearch(3), so here is a
   // new implementation.

   DBG_OBJ_MSGF ("container", 0,
                 "<b>bsearch</b> (<i>key</i>, %s, %d, %d, <i>comparator</i>) "
                 "[size is %d]",
                 mustExist ? "true" : "false", start, end, size ());
   DBG_OBJ_MSG_START ();

   int result = -123; // Compiler happiness: GCC 4.7 does not handle this?

   if (start > end) {
      DBG_OBJ_MSG ("container", 1, "empty");
      result = mustExist ? -1 : start;
   } else {
      int low = start, high = end;
      bool found = false;

      while (!found) {
         int index = (low + high) / 2;
         int c = comparator->compare (key, array[index]);
         DBG_OBJ_MSGF ("container", 1,
                       "searching within %d and %d; compare key with #%d => %d",
                       low, high, index, c);
         if (c == 0) {
            found = true;
            result = index;
         } else {
            if (low >= high) {
               if (mustExist) {
                  found = true;
                  result = -1;
               } else {
                  found = true;
                  result = c > 0 ? index + 1 : index;
               }
            }

            if (c < 0)
               high = index - 1;
            else
               low = index + 1;
         }
      }
   }

   DBG_OBJ_MSGF ("container", 1, "result = %d", result);
   DBG_OBJ_MSG_END ();
   return result;
}

Object *Vector::VectorIterator::getNext()
{
   if (index < vector->numElements - 1)
      index++;

   return index < vector->numElements ? vector->array[index] : NULL;
}

bool Vector::VectorIterator::hasNext()
{
   return index < vector->numElements - 1;
}

Collection0::AbstractIterator* Vector::createIterator()
{
   return new VectorIterator(this);
}

// ------------
//    List
// ------------

List::List(bool ownerOfObjects)
{
   this->ownerOfObjects = ownerOfObjects;
   first = last = NULL;
   numElements = 0;
}

List::~List()
{
   clear();
}

int List::size ()
{
   return numElements;
}

bool List::equals(Object *other)
{
   List *otherList = (List*)other;
   Node *node1 = first, *node2 = otherList->first;
   while (node1 != NULL && node2 != NULL ) {
      if (!node1->object->equals (node2->object))
         return false;
      node1 = node1->next;
      node2 = node2->next;
   }
   return node1 == NULL && node2 == NULL;
}

int List::hashValue()
{
   int h = 0;
   for (Node *node = first; node; node = node->next)
      h = h ^ node->object->hashValue ();
   return h;
}

void List::clear()
{
   while (first) {
      if (ownerOfObjects && first->object)
         delete first->object;
      Node *next = first->next;
      delete first;
      first = next;
   }

   last = NULL;
   numElements = 0;
}

void List::append(Object *element)
{
   Node *newLast = new Node;
   newLast->next = NULL;
   newLast->object = element;

   if (last) {
      last->next = newLast;
      last = newLast;
   } else
      first = last = newLast;

   numElements++;
}

bool List::insertBefore(object::Object *beforeThis, object::Object *neew)
{
   Node *beforeCur, *cur;

   for (beforeCur = NULL, cur = first; cur; beforeCur = cur, cur = cur->next) {
      if (cur->object == beforeThis) {
         Node *newNode = new Node;
         newNode->next = cur;
         newNode->object = neew;
         
         if (beforeCur)
            beforeCur->next = newNode;
         else
            first = newNode;

         numElements++;
         return true;
      }
   }

   return false;
}

bool List::remove0(Object *element, bool compare, bool doNotDeleteAtAll)
{
   Node *beforeCur, *cur;

   for (beforeCur = NULL, cur = first; cur; beforeCur = cur, cur = cur->next) {
      if (compare ?
          (cur->object && element->equals(cur->object)) :
          element == cur->object) {
         if (beforeCur) {
            beforeCur->next = cur->next;
            if (cur->next == NULL)
               last = beforeCur;
         } else {
            first = cur->next;
            if (first == NULL)
               last = NULL;
         }

         if (ownerOfObjects && cur->object && !doNotDeleteAtAll)
            delete cur->object;
         delete cur;

         numElements--;
         return true;
      }
   }

   return false;
}

Object *List::ListIterator::getNext()
{
   Object *object;

   if (current) {
      object = current->object;
      current = current->next;
   } else
      object = NULL;

   return object;
}

bool List::ListIterator::hasNext()
{
   return current != NULL;
}

Collection0::AbstractIterator* List::createIterator()
{
   return new ListIterator(first);
}


// ---------------
//    HashSet
// ---------------

HashSet::HashSet(bool ownerOfObjects, int tableSize)
{
   this->ownerOfObjects = ownerOfObjects;
   this->tableSize = tableSize;

   table = new Node*[tableSize];
   for (int i = 0; i < tableSize; i++)
      table[i] = NULL;

   numElements = 0;
}

HashSet::~HashSet()
{
   for (int i = 0; i < tableSize; i++) {
      Node *n1 = table[i];
      while (n1) {
         Node *n2 = n1->next;

         // It seems appropriate to call "clearNode(n1)" here instead
         // of "delete n1->object", but since this is the destructor,
         // the implementation of a sub class would not be called
         // anymore. This is the reason why HashTable has an
         // destructor.
         if (ownerOfObjects) {
            PRINTF ("- deleting object: %s\n", n1->object->toString());
            delete n1->object;
         }

         delete n1;
         n1 = n2;
      }
   }

   delete[] table;
}

int HashSet::size ()
{
   return numElements;
}

HashSet::Node *HashSet::createNode()
{
   return new Node;
}

void HashSet::clearNode(HashSet::Node *node)
{
   if (ownerOfObjects) {
      PRINTF ("- deleting object: %s\n", node->object->toString());
      delete node->object;
   }
}

HashSet::Node *HashSet::findNode(Object *object) const
{
   int h = calcHashValue(object);
   for (Node *node = table[h]; node; node = node->next) {
      if (object->equals(node->object))
         return node;
   }

   return NULL;
}

HashSet::Node *HashSet::insertNode(Object *object)
{
   // Look whether object is already contained.
   Node *node = findNode(object);
   if (node) {
      clearNode(node);
      numElements--;
   } else {
      int h = calcHashValue(object);
      node = createNode ();
      node->next = table[h];
      table[h] = node;
      numElements++;
   }

   node->object = object;
   return node;
}


void HashSet::put(Object *object)
{
   insertNode (object);
}

bool HashSet::contains(Object *object) const
{
   int h = calcHashValue(object);
   for (Node *n = table[h]; n; n = n->next) {
      if (object->equals(n->object))
         return true;
   }

   return false;
}

bool HashSet::remove(Object *object)
{
   int h = calcHashValue(object);
   Node *last, *cur;

   for (last = NULL, cur = table[h]; cur; last = cur, cur = cur->next) {
      if (object->equals(cur->object)) {
         if (last)
            last->next = cur->next;
         else
            table[h] = cur->next;

         clearNode (cur);
         delete cur;
         numElements--;

         return true;
      }
   }

   return false;

   // TODO for HashTable: also remove value.
}

// For historical reasons: this method once existed under the name
// "getKey" in HashTable. It could be used to get the real object from
// the table, but it was dangerous, because a change of this object
// would also change the hash value, and so confuse the table.

/*Object *HashSet::getReference (Object *object)
{
   int h = calcHashValue(object);
   for (Node *n = table[h]; n; n = n->next) {
      if (object->equals(n->object))
         return n->object;
   }

   return NULL;
}*/

HashSet::HashSetIterator::HashSetIterator(HashSet *set)
{
   this->set = set;
   node = NULL;
   pos = -1;
   gotoNext();
}

void HashSet::HashSetIterator::gotoNext()
{
   if (node)
      node = node->next;

   while (node == NULL) {
      if (pos >= set->tableSize - 1)
         return;
      pos++;
      node = set->table[pos];
   }
}


Object *HashSet::HashSetIterator::getNext()
{
   Object *result;
   if (node)
      result = node->object;
   else
      result = NULL;

   gotoNext();
   return result;
}

bool HashSet::HashSetIterator::hasNext()
{
   return node != NULL;
}

Collection0::AbstractIterator* HashSet::createIterator()
{
   return new HashSetIterator(this);
}

// ---------------
//    HashTable
// ---------------

HashTable::HashTable(bool ownerOfKeys, bool ownerOfValues, int tableSize) :
   HashSet (ownerOfKeys, tableSize)
{
   this->ownerOfValues = ownerOfValues;
}

HashTable::~HashTable()
{
   // See comment in the destructor of HashSet.
   for (int i = 0; i < tableSize; i++) {
      for (Node *n = table[i]; n; n = n->next) {
         if (ownerOfValues) {
            Object *value = ((KeyValuePair*)n)->value;
            if (value) {
               PRINTF ("- deleting value: %s\n", value->toString());
               delete value;
            }
         }
      }
   }
}

HashSet::Node *HashTable::createNode()
{
   return new KeyValuePair;
}

void HashTable::clearNode(HashSet::Node *node)
{
   HashSet::clearNode (node);
   if (ownerOfValues) {
      Object *value = ((KeyValuePair*)node)->value;
      if (value) {
         PRINTF ("- deleting value: %s\n", value->toString());
         delete value;
      }
   }
}

void HashTable::intoStringBuffer(misc::StringBuffer *sb)
{
   sb->append("{ ");

   bool first = true;
   for (int i = 0; i < tableSize; i++) {
      for (Node *node = table[i]; node; node = node->next) {
         if (!first)
            sb->append(", ");
         node->object->intoStringBuffer(sb);

         sb->append(" => ");

         Object *value = ((KeyValuePair*)node)->value;
         if (value)
             value->intoStringBuffer(sb);
         else
            sb->append ("null");

         first = false;
      }
   }

   sb->append(" }");
}

void HashTable::put(Object *key, Object *value)
{
   KeyValuePair *node = (KeyValuePair*)insertNode(key);
   node->value = value;
}

Object *HashTable::get(Object *key) const
{
   Node *node = findNode(key);
   if (node)
      return ((KeyValuePair*)node)->value;
   else
      return NULL;
}

// -----------
//    Stack
// -----------

Stack::Stack (bool ownerOfObjects)
{
   this->ownerOfObjects = ownerOfObjects;
   bottom = top = NULL;
   numElements = 0;
}

Stack::~Stack()
{
   while (top)
      pop ();
}

int Stack::size ()
{
   return numElements;
}

void Stack::push (object::Object *object)
{
   Node *newTop = new Node ();
   newTop->object = object;
   newTop->prev = top;

   top = newTop;
   if (bottom == NULL)
      bottom = top;

   numElements++;
}

void Stack::pushUnder (object::Object *object)
{
   Node *newBottom = new Node ();
   newBottom->object = object;
   newBottom->prev = NULL;
   if (bottom != NULL)
      bottom->prev = newBottom;

   bottom = newBottom;
   if (top == NULL)
      top = bottom;

   numElements++;
}

void Stack::pop ()
{
   Node *newTop = top->prev;

   if (ownerOfObjects)
      delete top->object;
   delete top;

   top = newTop;
   if (top == NULL)
      bottom = NULL;

   numElements--;
}

Object *Stack::StackIterator::getNext()
{
   Object *object;

   if (current) {
      object = current->object;
      current = current->prev;
   } else
      object = NULL;

   return object;
}

bool Stack::StackIterator::hasNext()
{
   return current != NULL;
}

Collection0::AbstractIterator* Stack::createIterator()
{
   return new StackIterator(top);
}

} // namespace untyped

} // namespace container

} // namespace lout
