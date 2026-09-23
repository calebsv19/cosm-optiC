#ifndef FISICS_EXTENSIONS_H
#define FISICS_EXTENSIONS_H

/*
 * Portable authoring spellings for fisiCs extension attributes.
 *
 * fisiCs defines __FISICS__ before preprocessing. Other C compilers see
 * these annotations as empty macros, so the same source can participate in
 * strict control-compiler builds without unknown-attribute warnings.
 */
#ifdef __FISICS__
#define FISICS_DIM(value) [[fisics::dim(value)]]
#define FISICS_UNIT(value) [[fisics::unit(value)]]
#define FISICS_ID(value) [[fisics::id(value)]]
#define FISICS_DOMAIN(value) [[fisics::domain(value)]]
#define FISICS_ROLE(value) [[fisics::role(value)]]
#define FISICS_PRODUCES(value) [[fisics::produces(value)]]
#define FISICS_CONSUMES(value) [[fisics::consumes(value)]]
#else
#define FISICS_DIM(value)
#define FISICS_UNIT(value)
#define FISICS_ID(value)
#define FISICS_DOMAIN(value)
#define FISICS_ROLE(value)
#define FISICS_PRODUCES(value)
#define FISICS_CONSUMES(value)
#endif

#endif
