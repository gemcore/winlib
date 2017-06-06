/*0===========================================================================
**
** File:       generic.hpp
**
** Purpose:    Pseudo C++ template macros.
**
**
**
**
** Date:       ?
**
** Author:     Zortech C/C++
**
**============================================================================
*/

#ifndef GENERIC_HPP
#define GENERIC_HPP

/* Name concatenator functions */

#define name2(n1,n2)                n1 ## n2
#define name3(n1,n2,n3)             n1 ## n2 ## n3
#define name4(n1,n2,n3,n4)          n1 ## n2 ## n3 ## n4

/* Psuedo 'template' declaration helper functions */

#define declare(a,type)             a##declare(type)
#define implement(a,type)           a##implement(type)
#define declare2(a,type1,type2)     a##declare2(type1,type2)
#define implement2(a,type1,type2)   a##implement2(type1,type2)

#endif
