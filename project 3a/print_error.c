/* NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */


#include <string.h>
#include <stdio.h>


void print_error(int errnum) {
  fprintf(stderr, "Error %d: %s", errnum, strerror(errnum));
}
