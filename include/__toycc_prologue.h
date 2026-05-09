#ifndef __TOYCC_PROLOGUE_H__
#define __TOYCC_PROLOGUE_H__

#define __restrict restrict
#define asm __asm__

#define __STDC_NO_ATOMICS__ 1
#define __STDC_NO_COMPLEX__ 1
#define __STDC_NO_THREADS__ 1
#define __STDC_NO_VLA__ 1

// At least for now, stick to standard C23 and disable glibc extensions
#define _ISOC23_SOURCE 1

#endif /* __TOYCC_PROLOGUE_H__ */
