/*****************************************************************************
*
* imd_forces -- force loop
*
******************************************************************************/

/******************************************************************************
* $RCSfile$
* $Revision$
* $Date$
******************************************************************************/

#include "imd.h"

/* ---------------------------------------------------------------------- 
*  do forces 2 ('t': two) and 3 ('t': three) body potential, risc version 
*
*  2&3 body potentials:
*	ttbp_1
*		2 body potential: no change, original imd code is used
*		3 body potential: search for neighbors
*	ttbp_2
*		3 body potential: calc potential / store force		 
*       ttbp_3  
*		3 body potential: calc force and virial                   
* ----------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */
void do_forces_ttbp_1(cell *p, cell *q, vektor pbc)

{
  vektor d, tmp_d, force;				         /* dummy */
#ifdef P_AXIAL
  vektor tmp_vir_vect;
#endif
  int    i,j,k;						       /* counter */
  int    q_typ,p_typ;					         /* sorte */
  int    jstart,jend;			                  /* ttbp counter */
  int    pni,pnj,tmp_k;	  		                    /* ttbp dummy */ 
  int    *tmp_ptri;			                    /* ttbp dummy */
  real   *tmp_ptr; 			                    /* ttbp dummy */
  real   radius2;					 /* distance ** 2 */
  real   r2_short = r2_end;				      /* distance */ 
  real   pot_zwi, pot_grad;				      /* pair pot */
  real   pot_k0,pot_k1,pot_k2,dv,d2v,chi;		      /* pair pot */
  real   *qptr;						         /* dummy */
  real   *potptr;					      /* pair pot */
  real   tmp_virial;				
 
  tmp_virial     = 0.0;
#ifdef P_AXIAL
  tmp_vir_vect.x = 0.0;
  tmp_vir_vect.y = 0.0;
  tmp_vir_vect.z = 0.0;
#endif

  /* For each atom in first cell */
  for (i = 0;i < p->n; ++i) {

    /* Some compilers don't find the expressions that are invariant 
       to the inner loop. I'll have to define my own temp variables. */

    tmp_d.x = p->ort X(i) - pbc.x;
    tmp_d.y = p->ort Y(i) - pbc.y;
    tmp_d.z = p->ort Z(i) - pbc.z;
    p_typ   = p->sorte[i];
    pni     = p->nummer[i];

    jstart  = (p==q ? i+1 : 0);
    qptr    = q->ort + DIM * jstart;
    
    /* For each atom in neighbouring cell */
    for (j = jstart; j < q->n; ++j) {
      
      /* Calculate distance  */
      d.x     = *qptr - tmp_d.x;
      ++qptr;
      d.y     = *qptr - tmp_d.y;
      ++qptr;
      d.z     = *qptr - tmp_d.z;
      ++qptr;
      radius2 = SPROD(d,d);
      q_typ   = q->sorte[j];
#ifndef NODBG_DIST
      if (0==radius2) { char msgbuf[256];
        sprintf(msgbuf,"Distance is zero: i=%d, j=%d\n",i,j);
        error(msgbuf);
      }
#else
      if (0==radius2) error("Distance is zero.");
#endif

      /* 1. Cutoff: Calculate force, if distance smaller than cutoff */ 
      if (radius2 <= r2_cut) {

      	/* Check for distances, shorter than minimal distance in pot. table */
	if (radius2 <= r2_0) {
	  radius2 = r2_0; 
	}; 

	/* Indices into potential table */
	k        = (int) ((radius2 - r2_0) * inv_r2_step);
	chi      = (radius2 - r2_0 - k * r2_step) * inv_r2_step;
	
	/* A single access to the potential table involves two multiplications 
	   We use a intermediate pointer to aviod this as much as possible.
	   Note: This relies on layout of the pot-table in memory!!! */

	potptr   = PTR_3D_V(potential, k, p_typ, q_typ, pot_dim);
	pot_k0   = *potptr; potptr += pot_dim.y + pot_dim.z;
	pot_k1   = *potptr; potptr += pot_dim.y + pot_dim.z;
	pot_k2   = *potptr;

	dv       = pot_k1 - pot_k0;
	d2v      = pot_k2 - 2 * pot_k1 + pot_k0;

	/* Norm of Gradient (contains minus sign) */
	pot_grad = 2 * inv_r2_step * ( dv + (chi - 0.5) * d2v );
        /* Potential energy of atom */
	pot_zwi  =  pot_k0 + chi * dv + 0.5 * chi * (chi - 1) * d2v;
        
	/* Store forces in temp (pot_grad is negative!) */
	force.x  = d.x * pot_grad;
	force.y  = d.y * pot_grad;
	force.z  = d.z * pot_grad;

        /* Accumulate forces */
	p->kraft X(i)  += force.x;
	p->kraft Y(i)  += force.y;
	q->kraft X(j)  -= force.x;
	q->kraft Y(j)  -= force.y;
	p->kraft Z(i)  += force.z;
	q->kraft Z(j)  -= force.z;
	q->pot_eng[j]  += pot_zwi;
	p->pot_eng[i]  += pot_zwi;
	tot_pot_energy += pot_zwi*2;

#ifdef P_AXIAL
        tmp_vir_vect.x -= d.x * d.x * pot_grad;
        tmp_vir_vect.y -= d.y * d.y * pot_grad;
        tmp_vir_vect.z -= d.z * d.z * pot_grad;
#else
        tmp_virial     -= radius2 * pot_grad;  
#endif

	/* 2. Cutoff: Three body potential neighbors; save atom# and x,y,z coords */
        if (radius2 <= ttbp_r2_cut[p_typ][q_typ]) {

	  pnj  = q->nummer[j];

	  /* i interacts with j: save #j in array k>0 */
	  ttbp_ij[pni*ttbp_len] += 1;
	  tmp_k     = ttbp_ij[pni*ttbp_len];
	  tmp_ptri  = &ttbp_ij[pni*ttbp_len+tmp_k*2-1];
	  *tmp_ptri = pnj; ++tmp_ptri;
	  *tmp_ptri = q_typ;
	  tmp_ptr   = &ttbp_j[pni*ttbp_len+tmp_k*3-2];
	  *tmp_ptr  = d.x; ++tmp_ptr; 
	  *tmp_ptr  = d.y; ++tmp_ptr; 
	  *tmp_ptr  = d.z;
	  /* j interacts with i: save #i in array k>0 */
	  ttbp_ij[pnj*ttbp_len] += 1;
	  tmp_k     = ttbp_ij[pnj*ttbp_len];
	  tmp_ptri  = &ttbp_ij[pnj*ttbp_len+tmp_k*2-1];
	  *tmp_ptri = pni; ++tmp_ptri;
	  *tmp_ptri = p_typ;
	  tmp_ptr   = &ttbp_j[pnj*ttbp_len+tmp_k*3-2];
	  *tmp_ptr  = -d.x; ++tmp_ptr; 
	  *tmp_ptr  = -d.y; ++tmp_ptr; 
	  *tmp_ptr  = -d.z;

	  if (tmp_k >= ttbp_len) error("increase ttbp_len!");
        }; /* if 2. Cutoff */ 

      }; /* if 1. Cutoff */

    }; /* for j */

  }; /* for i */

  /* A little security */
  if (r2_short < r2_0) 
    printf("\n Short distance! r2: %f\n",r2_short);

#ifdef P_AXIAL
  vir_vect.x += tmp_vir_vect.x;
  vir_vect.y += tmp_vir_vect.y;
  virial     += tmp_vir_vect.x;
  virial     += tmp_vir_vect.y;
  vir_vect.z += tmp_vir_vect.z;
  virial     += tmp_vir_vect.z;
#else
  virial     += tmp_virial;
#endif

  return;
}


/* ---------------------------------------------------------------------- */
void do_forces_ttbp_2(cell *p, cell *q, vektor pbc)

{
  vektor d,dd,tmp_d;
  int    i,j,k,s_k;					       /* counter */
  int    jstart,jend;					       /* counter */
  int	 pni,pnj,pnk,tmp_k;		                   /* atom number */	
  int    p_typ,j_typ,k_typ;				         /* sorte */
  int    *tmp_ptri;						 /* dummy */
  real   *tmp_ptr;						 /* dummy */
  real   pot_zwi,tmp_pot;				           /* pot */
  real   radius,radius2,rradius,rradius2;		      /* distance */
  real   x_j,y_j,z_j,x_k,y_k,z_k;  	                        /* coords */
  real   *s_potptr;				                   /* pot */
  real   s_chi,s_pot_k0,s_pot_k1,s_pot_k2;                         /* pot */
  real   s_dv,s_d2v;                                               /* pot */
  real   s_pot_grad_ik,s_pot_zwi_ik;		                   /* pot */
  real   s_pot_grad_ij,s_pot_zwi_ij,tmp_f2;	                   /* pot */
  real   theta;		                    /* actual value of theta(jik) */
  real   cos_theta;                      	    /* cosine(theta(jik)) */
  real   d_constant;	              /* harmonic: 2*k*(theta-ttbp_theta) */
  real   d_acos;	                        /* d cos(theta) / d theta */
  real   d_factor;	                             /* d_constant*d_acos */
  real   tmp_denom;	         /* inverse of denominator radius*rradius */
  real	 tmp_sp;	                    /* SPROD of vectors ij and ik */
  real   tmp_simple_r,tmp_simple_rr,tmp_A,tmp_B;                 /* dummy */
  real	 tmp_dcos_dxij,tmp_dcos_dxik;           /* dcos_theta / dx_(jk,i) */
  real	 tmp_dcos_dyij,tmp_dcos_dyik;           /* dcos_theta / dy_(jk,i) */
  real	 tmp_dcos_dzij,tmp_dcos_dzik;           /* dcos_theta / dz_(jk,i) */
  real   tmp_Fxij,tmp_Fxik,tmp_Fyij,tmp_Fyik,tmp_Fzij,tmp_Fzik;  /* force */

  /* The angle theta is the angle at atom i formed by the atoms j-i-k */

  /* For each atom in first cell */
  for (i = 0;i < p->n; ++i) {

    /* Some compilers don't find the expressions that are invariant 
       to the inner loop. I'll have to define my own temp variables. */

    /* pbc not necessary for tmp_d */
    tmp_d.x  = p->ort X(i);
    tmp_d.y  = p->ort Y(i);
    tmp_d.z  = p->ort Z(i);
    pni      = p->nummer[i];
    p_typ    = p->sorte[i];

    /* interaction of i with selected j's (from ttbp_1) */ 
    jstart   = 1;
    jend     = ttbp_ij[pni*ttbp_len];

    for (j = jstart; j < jend; ++j) {
   
      tmp_ptri = &ttbp_ij[pni*ttbp_len+j*2-1];
      pnj      = *tmp_ptri; ++tmp_ptri;
      j_typ    = *tmp_ptri;
      tmp_ptr  = &ttbp_j[pni*ttbp_len+j*3-2];
      d.x      = *tmp_ptr; ++tmp_ptr;
      d.y      = *tmp_ptr; ++tmp_ptr;
      d.z      = *tmp_ptr;

      /* Calculate distance ij */
      radius2  = SPROD(d,d);
      radius   = sqrt(radius2);
      /* Check for distances, shorter than minimal distance */
      if (radius2 <= ttbp_r2_0) {
        radius2  = ttbp_r2_0; 
        radius   = sqrt(radius2);
      }; 

#ifndef NODBG_DIST
      if (0==radius2) { char msgbuf[256];
        sprintf(msgbuf,"Distance is zero: i=%d, j=%d\n",i,j);
        error(msgbuf);
      }
#else
      if (0==radius2) error("Distance is zero.");
#endif

      /* ttbp smooth function ij: potential table */
      s_k      = (int) ((radius2 - ttbp_r2_0) * ttbp_inv_r2_step);
      s_chi    = (radius2 - ttbp_r2_0 - s_k * ttbp_r2_step) * ttbp_inv_r2_step;
	
      s_potptr = PTR_3D_V(ttbp_potential, s_k, p_typ, j_typ, pot_dim);
      s_pot_k0 = *s_potptr; s_potptr += pot_dim.y + pot_dim.z;
      s_pot_k1 = *s_potptr; s_potptr += pot_dim.y + pot_dim.z;
      s_pot_k2 = *s_potptr;

      s_dv     = s_pot_k1 - s_pot_k0;
      s_d2v    = s_pot_k2 - 2 * s_pot_k1 + s_pot_k0;

      s_pot_grad_ij = -2 * ttbp_inv_r2_step * ( s_dv + (s_chi - 0.5) * s_d2v );
      s_pot_zwi_ij  = s_pot_k0 + s_chi * s_dv + 0.5 * s_chi * (s_chi - 1) * s_d2v;

      /* interaction of i with selected j and j+N */
      for (k = j+1; k <= jend; ++k) {

        tmp_ptri = &ttbp_ij[pni*ttbp_len+k*2-1];
	pnk      = *tmp_ptri; ++tmp_ptri;
	k_typ    = *tmp_ptri;
        tmp_ptr  = &ttbp_j[pni*ttbp_len+k*3-2];
        dd.x     = *tmp_ptr; ++tmp_ptr;
        dd.y     = *tmp_ptr; ++tmp_ptr;
        dd.z     = *tmp_ptr;

        /* Calculate distance ik */
        rradius2  = SPROD(dd,dd);
	rradius   = sqrt(rradius2);
        /* Check for distances, shorter than minimal distance */
        if (rradius2 <= ttbp_r2_0) {
          rradius2 = ttbp_r2_0; 
	  rradius  = sqrt(rradius2);
        }; 

#ifndef NODBG_DIST
        if (0==rradius2) { char msgbuf[256];
          sprintf(msgbuf,"Distance is zero: i=%d, k=%d\n",i,k);
          error(msgbuf);
        }
#else
        if (0==rradius2) error("Distance is zero.");
#endif

        /* ttbp smooth function ij: potential table */
        s_k      = (int) ((rradius2 - ttbp_r2_0) * ttbp_inv_r2_step);
        s_chi    = (rradius2 - ttbp_r2_0 - s_k * ttbp_r2_step) * ttbp_inv_r2_step;
	
        s_potptr = PTR_3D_V(ttbp_potential, s_k, p_typ, k_typ , pot_dim);
        s_pot_k0 = *s_potptr; s_potptr += pot_dim.y + pot_dim.z;
        s_pot_k1 = *s_potptr; s_potptr += pot_dim.y + pot_dim.z;
        s_pot_k2 = *s_potptr;

        s_dv     = s_pot_k1 - s_pot_k0;
        s_d2v    = s_pot_k2 - 2 * s_pot_k1 + s_pot_k0;

        s_pot_grad_ik = -2 * ttbp_inv_r2_step * ( s_dv + (s_chi - 0.5) * s_d2v );
        s_pot_zwi_ik  = s_pot_k0 + s_chi * s_dv + 0.5 * s_chi * (s_chi - 1) * s_d2v;

	/* Calculate SP of vectors ij and ik */
	tmp_sp    = SPROD(d,dd);
	/* Calculate cosine(theta(jik)) */
	cos_theta = tmp_sp/radius/rradius;
	/* Calculate theta(jik) */
	theta     = acos(cos_theta);
	/* calculate f2 = f(ij)*f(ik) */
 	tmp_f2    = s_pot_zwi_ij * s_pot_zwi_ik;	                /* smooth */

	/* Energy (ttbp_theta[p_typ]: equilibrium angle) */
	/* Harmonic potential term */
	pot_zwi         = ttbp_constant[p_typ]*(theta-ttbp_theta[p_typ])
					      *(theta-ttbp_theta[p_typ]);
	tmp_pot		= pot_zwi ;
 	pot_zwi         = pot_zwi * tmp_f2 ;				/* smooth */

	p->pot_eng[i]  += pot_zwi;
	tot_pot_energy += pot_zwi;

	/* Forces ... the horror starts */

	/* Part 1: gradient = d E / d r */

	d_constant      = 2 * ttbp_constant[p_typ] * (theta - ttbp_theta[p_typ]);
	d_acos		= -1.0 / sin(theta);
	d_factor	= d_constant * d_acos;
 	d_factor        = d_factor * s_pot_grad_ij * s_pot_grad_ik;	/* smooth */

	/* Part 2a: d cos(theta) / d r(jk,i) */
	/* Part 2b: F = - grad E / d r * r/|r| = -d_factor * dcos_dr * r/|r| */

	tmp_denom	= 1.0/(radius*rradius);
	tmp_simple_r	= tmp_sp / radius2;
	tmp_simple_rr	= tmp_sp / rradius2;

	tmp_A     	= d.x  * tmp_simple_r;
	tmp_B     	= dd.x * tmp_simple_rr;
	tmp_dcos_dxij	= (dd.x - tmp_A) * tmp_denom ;
	 tmp_Fxij	= -d_factor * tmp_dcos_dxij * d.x  / radius  ;
	 tmp_Fxij	= tmp_Fxij  * tmp_f2  
	 		  -tmp_pot  * s_pot_zwi_ik  
	 		  	    * s_pot_grad_ij * d.x  ;		/* smooth */
	tmp_dcos_dxik	= (d.x  - tmp_B) * tmp_denom ;
	 tmp_Fxik	= -d_factor * tmp_dcos_dxik * dd.x / rradius ;
	 tmp_Fxik	= tmp_Fxik  * tmp_f2  
	  		  -tmp_pot  * s_pot_zwi_ij  
	  		            * s_pot_grad_ik * dd.x ;	  	/* smooth */

	tmp_A     	= d.y  * tmp_simple_r; 
	tmp_B     	= dd.y * tmp_simple_rr;
	tmp_dcos_dyij	= (dd.y - tmp_A) * tmp_denom ;
	 tmp_Fyij	= -d_factor * tmp_dcos_dyij * d.y  / radius  ;
	 tmp_Fyij	= tmp_Fyij  * tmp_f2 
	 		  -tmp_pot  * s_pot_zwi_ik  
	 		            * s_pot_grad_ij * d.y  ;		/* smooth */
	tmp_dcos_dyik	= (d.y  - tmp_B) * tmp_denom ;
	 tmp_Fyik	= -d_factor * tmp_dcos_dyik * dd.y / rradius ;
	 tmp_Fyik	= tmp_Fyik  * tmp_f2  
	 		  -tmp_pot  * s_pot_zwi_ij  
	 		            * s_pot_grad_ik * dd.y ;		/* smooth */

	tmp_A     	= d.z  * tmp_simple_r;
	tmp_B     	= dd.z * tmp_simple_rr;
	tmp_dcos_dzij	= (dd.z - tmp_A) * tmp_denom ;
	 tmp_Fzij	= -d_factor * tmp_dcos_dzij * d.z  / radius  ;
	 tmp_Fzij	= tmp_Fzij  * tmp_f2  
	 		  -tmp_pot  * s_pot_zwi_ik  
	 		            * s_pot_grad_ij * d.z  ;		/* smooth */
	tmp_dcos_dzik	= (d.z  - tmp_B) * tmp_denom ;
	 tmp_Fzik	= -d_factor * tmp_dcos_dzik * dd.z / rradius ;
	 tmp_Fzik	= tmp_Fzik  * tmp_f2  
	 		  -tmp_pot  * s_pot_zwi_ij  
	 		            * s_pot_grad_ik * dd.z ;		/* smooth */

	/* Part 3: store forces F = - grad E / d r(jk,i)  * r/|r| */
	tmp_ptr   = &ttbp_force[pni*3];
	*tmp_ptr -= tmp_Fxij + tmp_Fxik ; ++tmp_ptr;
	*tmp_ptr -= tmp_Fyij + tmp_Fyik ; ++tmp_ptr;
	*tmp_ptr -= tmp_Fzij + tmp_Fzik ;
	tmp_ptr   = &ttbp_force[pnj*3];
	*tmp_ptr += tmp_Fxij ;            ++tmp_ptr;
	*tmp_ptr += tmp_Fyij ;		  ++tmp_ptr;
	*tmp_ptr += tmp_Fzij ;		 
	tmp_ptr   = &ttbp_force[pnk*3];
	*tmp_ptr += tmp_Fxik ;		  ++tmp_ptr;
	*tmp_ptr += tmp_Fyik ;		  ++tmp_ptr;
	*tmp_ptr += tmp_Fzik ;		  

      }; /* for k */

    }; /* for j */

  }; /* for i */

  return;
}


/* ---------------------------------------------------------------------- */
void do_forces_ttbp_3(cell *p, cell *q, vektor pbc)

{
  int    i,j,k;						       /* counter */
  vektor tmp_d;				                      /* distance */
  int 	 pni; 	                                                 /* dummy */
  real   *tmp_ptr;						 /* dummy */
  real   tmp_virial;
#ifdef P_AXIAL
  vektor tmp_vir_vect;
#endif

  tmp_virial     = 0.0;
#ifdef P_AXIAL
  vektor tmp_vir_vect;
  tmp_vir_vect.x = 0.0;
  tmp_vir_vect.y = 0.0;
  tmp_vir_vect.z = 0.0;
#endif

  /* For each atom in first cell */
  for (i = 0;i < p->n; ++i) {

    tmp_d.x         = p->ort X(i);
    tmp_d.y         = p->ort Y(i);
    tmp_d.z         = p->ort Z(i);
    pni             = p->nummer[i];

    /* Accumulate forces (and dummy virials) */
    tmp_ptr = &ttbp_force[pni*3];
      	p->kraft X(i)  += *tmp_ptr; 
#ifdef P_AXIAL
      	tmp_vir_vect.x += tmp_d.x * *tmp_ptr;
#else
      	tmp_virial     += tmp_d.x * *tmp_ptr;
#endif
    ++tmp_ptr;
      	p->kraft Y(i)  += *tmp_ptr; 
#ifdef P_AXIAL
      	tmp_vir_vect.y += tmp_d.y * *tmp_ptr;
#else
      	tmp_virial     += tmp_d.y * *tmp_ptr;
#endif
    ++tmp_ptr;
      	p->kraft Z(i)  += *tmp_ptr; 
#ifdef P_AXIAL
      	tmp_vir_vect.z += tmp_d.z * *tmp_ptr;
#else
      	tmp_virial     += tmp_d.z * *tmp_ptr;
#endif

  }; /* for i */

#ifdef P_AXIAL
  vir_vect.x += tmp_vir_vect.x;
  vir_vect.y += tmp_vir_vect.y;
  virial     += tmp_vir_vect.x;
  virial     += tmp_vir_vect.y;
  vir_vect.z += tmp_vir_vect.z;
  virial     += tmp_vir_vect.z;
#else
  virial     += tmp_virial;
#endif

  return;
}
