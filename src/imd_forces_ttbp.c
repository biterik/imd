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
  int    jstart;			                  /* ttbp counter */
  int    pni,pnj,tmp_k=0;	  		            /* ttbp dummy */ 
  real   *tmp_ptri;			                    /* ttbp dummy */
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
    if (pni < 0 ) { pni = -pni; };
    jstart  = (p==q ? i+1 : 0);
    qptr    = q->ort + DIM * jstart;

    /* For each atom in neighbouring cell */
    for (j = jstart; j < q->n; ++j) {

      q_typ   = q->sorte[j];
      /* Calculate distance  */
      d.x     = *qptr - tmp_d.x; ++qptr;
      d.y     = *qptr - tmp_d.y; ++qptr;
      d.z     = *qptr - tmp_d.z; ++qptr;
      radius2 = SPROD(d,d);
      if (0==radius2) { char msgbuf[256];
        sprintf(msgbuf,"(ttbp_1) Distance is zero: i=%d (#%d), j=%d (#%d)\n",
		i,p->nummer[i],j,q->nummer[j]);
        error(msgbuf);
      }

      /* 1. Cutoff: Calculate force, if distance smaller than cutoff */ 
      if (radius2 <= r2_cut) {

      	/* Check for distances, shorter than minimal distance in pot. table */
	if (radius2 <= r2_0) { radius2 = r2_0; }; 

	/* Indices into potential table */
	k        = (int) ((radius2 - r2_0) * inv_r2_step);
	chi      = (radius2 - r2_0 - k * r2_step) * inv_r2_step;
	
	/* A single access to the potential table involves two multiplications 
	   We use a intermediate pointer to aviod this as much as possible.
	   Note: This relies on layout of the pot-table in memory!!! */
	potptr   = PTR_3D_V(potential, k, p_typ, q_typ, pot_dim);
	pot_k0   = *potptr; potptr += pot_dim.y * pot_dim.z;
	pot_k1   = *potptr; potptr += pot_dim.y * pot_dim.z;
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
	q->kraft X(j)  -= force.x;
	p->kraft Y(i)  += force.y;
	q->kraft Y(j)  -= force.y;
	p->kraft Z(i)  += force.z;
	q->kraft Z(j)  -= force.z;
	p->pot_eng[i]  += pot_zwi;
	q->pot_eng[j]  += pot_zwi;
	tot_pot_energy += pot_zwi*2;

#ifdef P_AXIAL
        tmp_vir_vect.x -= d.x * d.x * pot_grad;
        tmp_vir_vect.y -= d.y * d.y * pot_grad;
        tmp_vir_vect.z -= d.z * d.z * pot_grad;
#else
        tmp_virial     -= radius2   * pot_grad;  
#endif

	/* 2. Cutoff: Three body potential neighbors; save atom# and x,y,z coords */
        if (radius2 <= ttbp_r2_cut[p_typ][q_typ]) {

	  pnj  = q->nummer[j];
          if ( pnj < 0 ) { pnj = -pnj; };
	  /* i interacts with j: save #j in array k>0 */
          ttbp_ij[pni*ttbp_len] += (real) 1;
  	  tmp_k     = (int) ttbp_ij[pni*ttbp_len];
	  tmp_ptri  = &ttbp_ij[pni*ttbp_len+tmp_k*2-1];
	  *tmp_ptri = (real) pnj; ++tmp_ptri;
	  *tmp_ptri = (real) q_typ;
	  tmp_ptr   = &ttbp_j[pni*ttbp_len+tmp_k*3-2];
	  *tmp_ptr  = d.x; ++tmp_ptr; 
	  *tmp_ptr  = d.y; ++tmp_ptr; 
	  *tmp_ptr  = d.z;
	  /* j interacts with i: save #i in array k>0 */
	  ttbp_ij[pnj*ttbp_len] += (real) 1;
	  tmp_k     = (int) ttbp_ij[pnj*ttbp_len];
	  tmp_ptri  = &ttbp_ij[pnj*ttbp_len+tmp_k*2-1];
	  *tmp_ptri = (real) pni; ++tmp_ptri;
	  *tmp_ptri = (real) p_typ;
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
  vektor force,d,dd,tmp_d,dtheta_dr;
  int    i,j,k,s_k;					       /* counter */
  int    jstart,jend;					       /* counter */
  int	 pni,pnj,pnk;		                   	   /* atom number */	
  int    p_typ,j_typ,k_typ;				         /* sorte */
  real   *tmp_ptri, *tmp_ptr;				         /* dummy */
  real   dE_dr;
  real   pot_zwi,tmp_pot;				           /* pot */
  real   radius,radius2,rradius,rradius2;		      /* distance */
  real   *s_potptr;				                   /* pot */
  real   s_chi,s_pot_k0,s_pot_k1,s_pot_k2;                         /* pot */
  real   s_dv,s_d2v;                                               /* pot */
  real   s_pot_grad_ik,s_pot_zwi_ik;		                   /* pot */
  real   s_pot_grad_ij,s_pot_zwi_ij,tmp_f2;	                   /* pot */
  real   theta_grad;                /* actual value of theta(jik) in grad */
  real   theta_rad;               /* actual value of theta(jik) in radian */
  real   cos_theta;                      	    /* cosine(theta(jik)) */
  real   dE_dtheta;	                                   /* dE / dtheta */
  real   d_acos;	                        /* d cos(theta) / d theta */
  real   tmp_factor;	                             
  real   tmp_denom;	         /* inverse of denominator radius*rradius */
  real	 tmp_sp;	                    /* SPROD of vectors ij and ik */
  real   tmp_pi  = 3.141592654/180.0;                              /* ... */
  real   tmp_sin;                                                /* dummy */
  real   tmp_virial = 0.0;
#ifdef P_AXIAL
  vektor tmp_vir_vect;
  tmp_vir_vect.x = 0.0;
  tmp_vir_vect.y = 0.0;
  tmp_vir_vect.z = 0.0;
#endif

  /* The angle theta is the angle at atom i formed by the atoms j-i-k */
  /* For each atom in first cell */
  for (i = 0;i < p->n; ++i) {

    /* Some compilers don't find the expressions that are invariant 
       to the inner loop. I'll have to define my own temp variables. */
    pni      = p->nummer[i];
    if ( pni < 0 ) { pni = -pni; };
    p_typ    = p->sorte[i];
    /* interaction of i with selected j's (from ttbp_1) */ 
    jstart   = 1;
    jend     = (int) ttbp_ij[pni*ttbp_len];
    for (j = jstart; j < jend; ++j) {

      tmp_ptri = &ttbp_ij[pni*ttbp_len+j*2-1];
      pnj      = (int) *tmp_ptri; ++tmp_ptri;
      j_typ    = (int) *tmp_ptri;
      tmp_ptr  = &ttbp_j[pni*ttbp_len+j*3-2];
      d.x      = *tmp_ptr; ++tmp_ptr;
      d.y      = *tmp_ptr; ++tmp_ptr;
      d.z      = *tmp_ptr;

      /* Calculate distance ij */
      radius2  = SPROD(d,d);
      /* Check for distances, shorter than minimal distance */
      if (radius2 <= ttbp_r2_0) { radius2  = ttbp_r2_0; }; 
      radius   = sqrt(radius2);

      if (0==radius2) { char msgbuf[256];
        sprintf(msgbuf,"(ttbp_2) Distance is zero: i=%d (#%d), j=%d (#%d)\n",
		i,p->nummer[i],j,q->nummer[j]);
        error(msgbuf);
      }

      /* ttbp smooth function ij: potential table */
      s_k      = (int) ((radius2 - ttbp_r2_0) * ttbp_inv_r2_step);
      s_chi    = (radius2 - ttbp_r2_0 - s_k * ttbp_r2_step) * ttbp_inv_r2_step;
      s_potptr = PTR_3D_V(ttbp_potential, s_k, p_typ, j_typ, ttbp_pot_dim);
      s_pot_k0 = *s_potptr; s_potptr += ttbp_pot_dim.y * ttbp_pot_dim.z;
      s_pot_k1 = *s_potptr; s_potptr += ttbp_pot_dim.y * ttbp_pot_dim.z;
      s_pot_k2 = *s_potptr;
      s_dv     = s_pot_k1 - s_pot_k0;
      s_d2v    = s_pot_k2 - 2 * s_pot_k1 + s_pot_k0;
      s_pot_grad_ij = -2 * ttbp_inv_r2_step * ( s_dv + (s_chi - 0.5) * s_d2v );
      s_pot_zwi_ij  = s_pot_k0 + s_chi * s_dv + 0.5 * s_chi * (s_chi - 1) * s_d2v;

      /* interaction of i with selected j and j+N */
      for (k = j+1; k <= jend; ++k) {

        tmp_ptri = &ttbp_ij[pni*ttbp_len+k*2-1];
	pnk      = (int) *tmp_ptri; ++tmp_ptri;
	k_typ    = (int) *tmp_ptri;
        tmp_ptr  = &ttbp_j[pni*ttbp_len+k*3-2];
        dd.x     = *tmp_ptr; ++tmp_ptr;
        dd.y     = *tmp_ptr; ++tmp_ptr;
        dd.z     = *tmp_ptr;

        /* Calculate distance ik */
        rradius2  = SPROD(dd,dd);
        /* Check for distances, shorter than minimal distance */
        if (rradius2 <= ttbp_r2_0) { rradius2 = ttbp_r2_0; }; 
	rradius  = sqrt(rradius2);
        if (0==rradius2) { char msgbuf[256];
          sprintf(msgbuf,"(ttbp_2) Distance is zero: i=%d (#%d), k=%d (#%d)\n",
		  i,pni,k,pnk);
          error(msgbuf);
        }

        /* ttbp smooth function ik: potential table */
        s_k      = (int) ((rradius2 - ttbp_r2_0) * ttbp_inv_r2_step);
        s_chi    = (rradius2 - ttbp_r2_0 - s_k * ttbp_r2_step) * ttbp_inv_r2_step;
        s_potptr = PTR_3D_V(ttbp_potential, s_k, p_typ, k_typ , ttbp_pot_dim);
        s_pot_k0 = *s_potptr; s_potptr += ttbp_pot_dim.y * ttbp_pot_dim.z;
        s_pot_k1 = *s_potptr; s_potptr += ttbp_pot_dim.y * ttbp_pot_dim.z;
        s_pot_k2 = *s_potptr;
        s_dv     = s_pot_k1 - s_pot_k0;
        s_d2v    = s_pot_k2 - 2 * s_pot_k1 + s_pot_k0;
        s_pot_grad_ik = -2 * ttbp_inv_r2_step * ( s_dv + (s_chi - 0.5) * s_d2v );
        s_pot_zwi_ik  = s_pot_k0 + s_chi * s_dv + 0.5 * s_chi * (s_chi - 1) * s_d2v;

	/* Calculate SP of vectors ij and ik */
	tmp_sp    = SPROD(d,dd);
	/* Calculate cosine(theta(jik)) */
	cos_theta = tmp_sp/radius/rradius;

        /*  incase some security is needed:
        if (cos_theta >  1.0) { cos_theta =  1.0; };
        if (cos_theta < -1.0) { cos_theta = -1.0; };
        */

	/* Calculate theta(jik) */
	theta_rad = acos(cos_theta);
	/* calculate f2 = f(ij)*f(ik) */
 	tmp_f2    = s_pot_zwi_ij * s_pot_zwi_ik;	                /* smooth */

        /* FOURIER potential term */
        pot_zwi = ttbp_constant[p_typ]*(ttbp_c0[p_typ] +
                                        ttbp_c1[p_typ] * cos_theta +
                                        ttbp_c2[p_typ] * cos(2*theta_rad) +
                                        ttbp_c3[p_typ] * cos(3*theta_rad));
	tmp_pot		= pot_zwi ;
 	pot_zwi         = pot_zwi * tmp_f2 ;				/* smooth */
	p->pot_eng[i]  += pot_zwi;
	tot_pot_energy += pot_zwi;

	/* Forces ... the horror starts 
	 * F = - d E / d r  
	 *   = - d E / d Theta  *  d Theta / d r
	 *   = - d E / d Theta  *  d_acos *  dtheta_dr 
	 * smoothing 
	 * F = - d (E*Fij*Fik) / d r
	 *   = - d E / d r * Fij * Fik   - E * d Fij / d r * Fik  - E * Fij * d Fik / d r
	 */

        dE_dtheta    =  - ttbp_constant[p_typ]*(  ttbp_c1[p_typ]  * sin(theta_rad) + 
                                                2*ttbp_c2[p_typ]  * sin(2*theta_rad) + 
                                                3*ttbp_c3[p_typ]  * sin(3*theta_rad));

        tmp_sin = sin(theta_rad);
        /*  in case some security is needed
        if ( (tmp_sin <  0.0001) && (tmp_sin >= 0.0) ) { tmp_sin =  0.0001; };
        if ( (tmp_sin > -0.0001) && (tmp_sin <= 0.0) ) { tmp_sin = -0.0001; };
        */
        d_acos          = - 1.0 / tmp_sin;
	tmp_denom	=   1.0 / (radius*rradius);
	tmp_factor	= - tmp_denom * d_acos;

	dtheta_dr.x	= tmp_factor * ( d.x  * ( tmp_sp / radius2  - 1) +
			                 dd.x * ( tmp_sp / rradius2 - 1) ) ;
	dtheta_dr.y	= tmp_factor * ( d.y  * ( tmp_sp / radius2  - 1) +
			                 dd.y * ( tmp_sp / rradius2 - 1) ) ;
	dtheta_dr.z	= tmp_factor * ( d.z  * ( tmp_sp / radius2  - 1) +
			                 dd.z * ( tmp_sp / rradius2 - 1) ) ;

	force.x  = - dE_dtheta * dtheta_dr.x * tmp_f2
	           + tmp_pot   * (  s_pot_grad_ij * d.x   * s_pot_zwi_ik
	                          + s_pot_grad_ik * dd.x  * s_pot_zwi_ij);
	force.y  = - dE_dtheta * dtheta_dr.y * tmp_f2
	           + tmp_pot   * (  s_pot_grad_ij * d.y   * s_pot_zwi_ik
	                          + s_pot_grad_ik * dd.y  * s_pot_zwi_ij);
	force.z  = - dE_dtheta * dtheta_dr.z * tmp_f2
	           + tmp_pot   * (  s_pot_grad_ij * d.z   * s_pot_zwi_ik
	                          + s_pot_grad_ik * dd.z  * s_pot_zwi_ij);
	p->kraft X(i)  += force.x;
	p->kraft Y(i)  += force.y;
	p->kraft Z(i)  += force.z;

        tmp_d.x         = p->ort X(i);
    	tmp_d.y         = p->ort Y(i);
    	tmp_d.z         = p->ort Z(i);
#ifdef P_AXIAL
        tmp_vir_vect.x -= tmp_d.x * force.x;
        tmp_vir_vect.y -= tmp_d.y * force.y;
        tmp_vir_vect.z -= tmp_d.z * force.z;
#else
        tmp_virial     -= SPROD(tmp_d,force);
#endif

      }; /* for k */
    }; /* for j */
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
