/******************************************************************************
*
* config.h -- Configuration rules and constants for the imd Package
*
******************************************************************************/

/******************************************************************************
* $RCSfile$
* $Revision$
* $Date$
******************************************************************************/

/******************************************************************************
*
* Misc. Settings
*
******************************************************************************/

/* Double Precision is default */
#ifndef SINGLE
#define DOUBLE
#endif

/* define SAVEMEM for small cell structure            */
/* define AR for using actio-reactio principle        */
/* define USE_RUSAGE for using getrusage()-routines   */
/* define USE_CLOCK to use clock() instead of times() */

#ifdef MONOLJ
#define SAVEMEM
#undef AR
#define NODBG_DIST
#endif

/******************************************************************************
*
* Statistical Ensembles
*
******************************************************************************/

/* Microcanonical */

#ifdef NVE

#endif



/* Constant Temperature & Pressure */

#ifdef NPT
#define DYNAMIC_CELLS
#endif

#ifdef NPT_axial
#define P_AXIAL
#define PAXTEST
#endif

/* Canonical */

#ifdef NVT

#endif


/* heat transport */
#ifdef NVX
#define RNEMD
#endif
#if defined(RNEMD) && !defined(NVX)
/* #define NVE */
#endif
#if defined(NVX) || defined(RNEMD)
#define TRANSPORT
#endif

/* Stadium damping for fracture */

#ifdef FRAC
#define NOPBC
#endif

#ifdef DEFORM
/* #define NOPBC  */
#endif

#ifdef MSQD
#define REFPOS
#endif

#ifdef CORRELATE
#define REFPOS
#endif

#ifdef SHOCK
#define STRESS_TENS
#endif

#ifdef EAM
#undef AR
#endif

#ifdef EAM2
#undef AR
#endif

#if defined(TTBP) || defined(TERSOFF)
#define COVALENT
#endif

#ifdef COVALENT
#ifndef AR
#define AR
#endif
#endif

#ifdef UNIAX
#undef AR
#endif

/******************************************************************************
*
* Architectures
*
******************************************************************************/

/* Single CPU, Workstation */

#ifdef RISC

#endif

/* Parallel Machine using Message Passing Interface */

#ifdef MPI

#endif

#ifdef CRAY 

#endif

#ifdef SX4
#undef USE_SOCKETS
#endif

/* Metacomputing */

#ifdef PACX
/* #define MPI_Ssend MPI_Send */
#endif

/******************************************************************************
*
* Constants
*
******************************************************************************/

/* memory allocation increment for potential */
#define PSTEP 50

/* security margin for buffer sizes */
#define CSTEP 10

