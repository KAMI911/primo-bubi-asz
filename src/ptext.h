#ifndef PTEXT_H
#define PTEXT_H
/* Interface text in two languages, chosen at build time (-DLANG_HU). Upper case, no accents: the Primo's font is ASCII. */
#ifdef LANG_HU
#define T(en, hu) hu
#else
#define T(en, hu) en
#endif
#endif
