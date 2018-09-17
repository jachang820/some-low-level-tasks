/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */ 


/** Note: This is probably horrible code architecture, but
 *        it's this only way I can think of without
 *        re-designing the entire program.
 *
 *        I'd like to be able to pass the sublist number to
 *        SortedList_(functions) since I've previously 
 *        decided to put sync options in that file. Since 
 *        there are multiple sublists now, I need multiple 
 *        sync objects, but I have no way of keeping track 
 *        of which sync object to use. I cannot change the 
 *        function parameters.
 *        
 *        My solution is to use a dummy struct holding a
 *        SortedList_t or SortedListElement_t pointer and
 *        the sublist number. A pointer to the struct will
 *        be cast as SortedList_t and passed into, e.g., 
 *        SortedList_insert(), and then re-cast into its
 *        original type, so the list and sublist number
 *        could be extracted.
 */

extern int num_lists;

struct ListInfo {
  void *list_obj;
  long long timer;
  int bin;
};
