/*
 * objc.h — Objective-C runtime pro CCC
 */

#ifndef _CCC_OBJC_H
#define _CCC_OBJC_H

typedef void *id;
typedef void *SEL;
typedef void *Class;
typedef char BOOL;

#define YES ((BOOL)1)
#define NO  ((BOOL)0)
#define nil ((id)0)

/* runtime */
Class objc_getClass(const char *name);
SEL sel_registerName(const char *str);
const char *sel_getName(SEL sel);
const char *class_getName(Class cls);
Class object_getClass(id obj);
id objc_retain(id obj);
void objc_release(id obj);
id objc_autorelease(id obj);
id objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(id pool);

/*
 * objc_msgSend — ARM64: declaratur ut void(void), NON variadica.
 * Vocans DEBET castare ad signaturam exactam methodi.
 *
 * Exemplum:
 *   typedef id (*msg0)(id, SEL);
 *   id app = ((msg0)objc_msgSend)(cls, sel);
 *
 *   typedef id (*msg1)(id, SEL, id);
 *   ((msg1)objc_msgSend)(obj, sel, arg);
 */
void objc_msgSend(void);
void objc_msgSend_stret(void);

#endif /* _CCC_OBJC_H */
