/*
 * Copyright 1997, Regents of the University of Minnesota
 *
 * xyzpart.c
 *
 * This file contains code that implements a coordinate based partitioning
 *
 * Started 7/11/97
 * George
 *
 * $Id: xyzpart.c 10592 2011-07-16 21:17:53Z karypis $
 *
 */

#include <parmetislib.h>


/*************************************************************************/
/*! This function implements a simple coordinate based partitioning
*/
/*************************************************************************/
void Coordinate_Partition(ctrl_t *ctrl, graph_t *graph, idx_t ndims, 
         real_t *xyz, idx_t setup)
{
  idx_t i, j, k, nvtxs, firstvtx, icoord, nbits; 
  idx_t *vtxdist, *bxyz;
  ikv_t *cand;

  WCOREPUSH;

  if (setup)
    CommSetup(ctrl, graph);
  else
    graph->nrecv = 0;

  nvtxs    = graph->nvtxs;
  vtxdist  = graph->vtxdist;
  firstvtx = vtxdist[ctrl->mype];

  cand = ikvwspacemalloc(ctrl, nvtxs);
  bxyz = iwspacemalloc(ctrl, nvtxs*ndims);

  /* Assign the coordinates into bins */
  nbits = 9;  /* 2^nbits # of bins */
  IRBinCoordinates(ctrl, graph, ndims, xyz, 1<<nbits, bxyz);

  /* Z-ordering */
  for (i=0; i<nvtxs; i++) {
    for (icoord=0, j=nbits-1; j>=0; j--) {
      for (k=0; k<ndims; k++)
        icoord = (icoord<<1) + (bxyz[i*ndims+k]&(1<<j) ? 1 : 0);
    }
    cand[i].key = icoord;
    cand[i].val = firstvtx+i;
  }


  /* Partition using sorting */
  PseudoSampleSort(ctrl, graph, cand);

  WCOREPOP;
}


/*************************************************************************/
/*! This function maps the coordinates into bin numbers.
    It starts with a uniform distribution of the max-min range and then
    performs a number of iterations that adjust the bucket boundaries based
    on the actual bucket counts.
*/
/*************************************************************************/
void IRBinCoordinates(ctrl_t *ctrl, graph_t *graph, idx_t ndims, real_t *xyz, 
         idx_t nbins, idx_t *bxyz)
{
  idx_t npes=ctrl->npes, mype=ctrl->mype; 
  idx_t i, j, k, l, gnvtxs, nvtxs;
  idx_t csize, psize;
  idx_t *vtxdist, *lcounts, *gcounts;
  real_t gmin, gmax, *emarkers, *nemarkers;
  rkv_t *cand;

  WCOREPUSH;

  gnvtxs   = graph->gnvtxs;
  nvtxs    = graph->nvtxs;

  cand      = rkvwspacemalloc(ctrl, nvtxs);
  lcounts   = iwspacemalloc(ctrl, nbins);
  gcounts   = iwspacemalloc(ctrl, nbins);
  emarkers  = rwspacemalloc(ctrl, nbins+1);
  nemarkers = rwspacemalloc(ctrl, nbins+1);


  /* Go over each dimension */
  for (k=0; k<ndims; k++) {
    for (i=0; i<nvtxs; i++) {
      cand[i].key = xyz[i*ndims+k];
      cand[i].val = i;
    }
    rkvsorti(nvtxs, cand);

    /* determine initial range */
    gkMPI_Allreduce((void *)&cand[0].key, (void *)&gmin, 1, REAL_T, MPI_MIN, ctrl->comm);
    gkMPI_Allreduce((void *)&cand[nvtxs-1].key, (void *)&gmax, 1, REAL_T, MPI_MAX, ctrl->comm);

    for (i=0; i<nbins; i++)
      emarkers[i] = gmin + (gmax-gmin)*i/nbins;
    emarkers[nbins] = gmax*(1.0+2.0*REAL_EPSILON);

    /* get into a iterative backet boundary refinement */
    for (l=0; l<5; l++) {
      /* determine bucket counts */
      iset(nbins, 0, lcounts);
      for (j=0, i=0; i<nvtxs;) {
        if (cand[i].key < emarkers[j+1]) {
          lcounts[j]++;
          i++;
        }
        else {
          j++;
        }
      }

      gkMPI_Allreduce((void *)lcounts, (void *)gcounts, nbins, IDX_T, MPI_SUM, ctrl->comm);

      /*
      if (mype == 0) {
        printf("Distribution [%"PRIDX"]...\n", l);
        for (i=0; i<nbins; i++)
          printf("\t%"PRREAL" - %"PRREAL" => %"PRIDX"\n", emarkers[i], emarkers[i+1], gcounts[i]);
      }
      */

      /* break-out if things look reasonably balanced */
      if (imax(nbins, gcounts) < 4*gnvtxs/nbins)
        break;

      /* refine buckets */
      rset(nbins, -1, nemarkers);
      for (j=0, i=0; i<nbins; i++) {
        for (csize=0; ; j++) {
          if (csize+gcounts[j] < gnvtxs/nbins) {
            csize += gcounts[j];
          }
          else {
            psize = gnvtxs/nbins-csize;
            emarkers[j] += (emarkers[j+1]-emarkers[j])*psize/gcounts[j];
            gcounts[j]  -= psize;

            nemarkers[i+1] = emarkers[j];
            break;
          }
        }
      }
      nemarkers[0]     = gmin;
      nemarkers[nbins] = gmax*(1.0+2.0*REAL_EPSILON);
      rcopy(nbins+1, nemarkers, emarkers);
    }

    /* assign the coordinate to the appropriate bin */
    for (j=0, i=0; i<nvtxs;) {
      if (cand[i].key < emarkers[j+1]) {
        bxyz[cand[i].val*ndims+k] = j;
        i++;
      }
      else {
        j++;
      }
    }
  }

  WCOREPOP;
}


/*************************************************************************/
/*! This function maps the coordinates into bin numbers. It uses a per
    dimension recursive center-of mass bisection approach.
*/
/*************************************************************************/
void RBBinCoordinates(ctrl_t *ctrl, graph_t *graph, idx_t ndims, real_t *xyz, 
         idx_t nbins, idx_t *bxyz)
{
  idx_t npes=ctrl->npes, mype=ctrl->mype; 
  idx_t i, j, k, l, gnvtxs, nvtxs, cnbins;
  idx_t *vtxdist, *lcounts, *gcounts;
  real_t sum, gmin, gmax, gsum, *emarkers, *nemarkers, *lsums, *gsums;
  rkv_t *cand;
  ikv_t *buckets;

  WCOREPUSH;

  gnvtxs   = graph->gnvtxs;
  nvtxs    = graph->nvtxs;

  buckets   = ikvwspacemalloc(ctrl, nbins);
  cand      = rkvwspacemalloc(ctrl, nvtxs);
  lcounts   = iwspacemalloc(ctrl, nbins);
  gcounts   = iwspacemalloc(ctrl, nbins);
  lsums     = rwspacemalloc(ctrl, nbins);
  gsums     = rwspacemalloc(ctrl, nbins);
  emarkers  = rwspacemalloc(ctrl, nbins+1);
  nemarkers = rwspacemalloc(ctrl, nbins+1);


  /* Go over each dimension */
  for (k=0; k<ndims; k++) {
    for (sum=0.0, i=0; i<nvtxs; i++) {
      cand[i].key = xyz[i*ndims+k];
      cand[i].val = i;
      sum += cand[i].key;
    }
    rkvsorti(nvtxs, cand);

    /* determine initial stats */
    gkMPI_Allreduce((void *)&cand[0].key, (void *)&gmin, 1, REAL_T, MPI_MIN, ctrl->comm);
    gkMPI_Allreduce((void *)&cand[nvtxs-1].key, (void *)&gmax, 1, REAL_T, MPI_MAX, ctrl->comm);
    gkMPI_Allreduce((void *)&sum, (void *)&gsum, 1, REAL_T, MPI_MAX, ctrl->comm);

    emarkers[0] = gmin;
    emarkers[1] = gsum/gnvtxs;
    emarkers[2] = gmax*(1.0+2.0*REAL_EPSILON);
    cnbins = 2;

    /* get into a iterative backet boundary refinement */
    while (cnbins < nbins) {
      /* determine bucket counts */
      iset(cnbins, 0, lcounts);
      rset(cnbins, 0, lsums);
      for (j=0, i=0; i<nvtxs;) {
        if (cand[i].key < emarkers[j+1]) {
          lcounts[j]++;
          lsums[j] += cand[i].key;
          i++;
        }
        else {
          j++;
        }
      }

      gkMPI_Allreduce((void *)lcounts, (void *)gcounts, cnbins, IDX_T, MPI_SUM, ctrl->comm);
      gkMPI_Allreduce((void *)lsums, (void *)gsums, cnbins, REAL_T, MPI_SUM, ctrl->comm);

      /*
      if (mype == 0) {
        printf("Distribution [%"PRIDX"]...\n", cnbins);
        for (i=0; i<cnbins; i++)
          printf("\t%"PRREAL" - %"PRREAL" => %"PRIDX"\n", emarkers[i], emarkers[i+1], gcounts[i]);
      }
      */


      /* split over-weight buckets */
      for (i=0; i<cnbins; i++) {
        buckets[i].key = gcounts[i];
        buckets[i].val = i;
      }
      ikvsorti(cnbins, buckets);

      for (j=0, i=cnbins-1; i>=0; i--, j++) {
        l = buckets[i].val;
        if (buckets[i].key > gnvtxs/nbins && cnbins < nbins) {
          /*
          if (mype == 0)
            printf("\t\t %f %f\n", (float)emarkers[l], (float)emarkers[l+1]);
          */
          nemarkers[j++] = (emarkers[l]+emarkers[l+1])/2;
          cnbins++;
        }
        nemarkers[j] = emarkers[l];
      }
      PASSERT(ctrl, cnbins == j);
      
      rsorti(cnbins, nemarkers);
      rcopy(cnbins, nemarkers, emarkers);
      emarkers[cnbins] = gmax*(1.0+2.0*REAL_EPSILON);
    }

    /* assign the coordinate to the appropriate bin */
    for (j=0, i=0; i<nvtxs;) {
      if (cand[i].key < emarkers[j+1]) {
        bxyz[cand[i].val*ndims+k] = j;
        i++;
      }
      else {
        j++;
      }
    }
  }

  WCOREPOP;
}


/**************************************************************************/
/*! This function sorts a distributed list of ikv_t in increasing 
    order, and uses it to compute a partition. It uses samplesort. 

    This function is poorly implemented and makes the assumption that the
    number of vertices in each processor is greater than npes. 
    This constraint is currently enforced by the calling functions. 
    \todo fix it in 4.0.
*/
/**************************************************************************/
void SampleSort(ctrl_t *ctrl, graph_t *graph, ikv_t *elmnts)
{
  idx_t i, j, k, nvtxs, nrecv, npes=ctrl->npes, mype=ctrl->mype, 
        firstvtx, lastvtx;
  idx_t *scounts, *rcounts, *vtxdist, *perm;
  ikv_t *relmnts, *mypicks, *allpicks;

  WCOREPUSH;

  CommUpdateNnbrs(ctrl, npes);

  nvtxs   = graph->nvtxs;
  vtxdist = graph->vtxdist;

  /* get memory for the counts */
  scounts = iwspacemalloc(ctrl, npes+1);
  rcounts = iwspacemalloc(ctrl, npes+1);

  /* get memory for the splitters */
  mypicks  = ikvwspacemalloc(ctrl, npes+1);
  WCOREPUSH; /* for freeing allpicks */
  allpicks = ikvwspacemalloc(ctrl, npes*npes);

  /* Sort the local elements */
  ikvsorti(nvtxs, elmnts);

  /* Select the local npes-1 equally spaced elements */
  for (i=1; i<npes; i++) { 
    mypicks[i-1].key = elmnts[i*(nvtxs/npes)].key;
    mypicks[i-1].val = elmnts[i*(nvtxs/npes)].val;
  }

  /* PrintPairs(ctrl, npes-1, mypicks, "Mypicks"); */

  /* Gather the picks to all the processors */
  gkMPI_Allgather((void *)mypicks, 2*(npes-1), IDX_T, (void *)allpicks, 
      2*(npes-1), IDX_T, ctrl->comm);

  /* PrintPairs(ctrl, npes*(npes-1), allpicks, "Allpicks"); */

  /* Sort all the picks */
  ikvsortii(npes*(npes-1), allpicks);

  /* PrintPairs(ctrl, npes*(npes-1), allpicks, "Allpicks"); */

  /* Select the final splitters. Set the boundaries to simplify coding */
  for (i=1; i<npes; i++)
    mypicks[i] = allpicks[i*(npes-1)];
  mypicks[0].key    = IDX_MIN;
  mypicks[npes].key = IDX_MAX;

  /* PrintPairs(ctrl, npes+1, mypicks, "Mypicks"); */

  WCOREPOP;  /* free allpicks */

  /* Compute the number of elements that belong to each bucket */
  iset(npes, 0, scounts);
  for (j=i=0; i<nvtxs; i++) {
    if (elmnts[i].key < mypicks[j+1].key || 
        (elmnts[i].key == mypicks[j+1].key && elmnts[i].val < mypicks[j+1].val))
      scounts[j]++;
    else
      scounts[++j]++;
  }
  gkMPI_Alltoall(scounts, 1, IDX_T, rcounts, 1, IDX_T, ctrl->comm);

  MAKECSR(i, npes, scounts);
  MAKECSR(i, npes, rcounts);

/*
  PrintVector(ctrl, npes+1, 0, scounts, "Scounts");
  PrintVector(ctrl, npes+1, 0, rcounts, "Rcounts");
*/

  /* Allocate memory for sorted elements and receive them */
  nrecv   = rcounts[npes];
  relmnts = ikvwspacemalloc(ctrl, nrecv);

  /* Issue the receives first */
  for (i=0; i<npes; i++) 
    gkMPI_Irecv((void *)(relmnts+rcounts[i]), 2*(rcounts[i+1]-rcounts[i]), 
        IDX_T, i, 1, ctrl->comm, ctrl->rreq+i);

  /* Issue the sends next */
  for (i=0; i<npes; i++) 
    gkMPI_Isend((void *)(elmnts+scounts[i]), 2*(scounts[i+1]-scounts[i]), 
        IDX_T, i, 1, ctrl->comm, ctrl->sreq+i);

  gkMPI_Waitall(npes, ctrl->rreq, ctrl->statuses);
  gkMPI_Waitall(npes, ctrl->sreq, ctrl->statuses);


  /* OK, now do the local sort of the relmnts. Use perm to keep track original order */
  perm = iwspacemalloc(ctrl, nrecv);
  for (i=0; i<nrecv; i++) {
    perm[i]        = relmnts[i].val;
    relmnts[i].val = i;
  }
  ikvsorti(nrecv, relmnts);


  /* Compute what needs to be shifted */
  gkMPI_Scan((void *)(&nrecv), (void *)(&lastvtx), 1, IDX_T, MPI_SUM, ctrl->comm);
  firstvtx = lastvtx-nrecv;  

  /*myprintf(ctrl, "first, last: %"PRIDX" %"PRIDX"\n", firstvtx, lastvtx); */

  for (j=0, i=0; i<npes; i++) {
    if (vtxdist[i+1] > firstvtx) {  /* Found the first PE that is passed me */
      if (vtxdist[i+1] >= lastvtx) {
        /* myprintf(ctrl, "Shifting %"PRIDX" elements to processor %"PRIDX"\n", lastvtx-firstvtx, i); */
        for (k=0; k<lastvtx-firstvtx; k++, j++) 
          relmnts[relmnts[j].val].key = i;
      }
      else {
        /* myprintf(ctrl, "Shifting %"PRIDX" elements to processor %"PRIDX"\n", vtxdist[i+1]-firstvtx, i); */
        for (k=0; k<vtxdist[i+1]-firstvtx; k++, j++) 
          relmnts[relmnts[j].val].key = i;

        firstvtx = vtxdist[i+1];
      }
    }
    if (vtxdist[i+1] >= lastvtx)
      break;
  }

  /* Reverse the ordering on the relmnts[].val */
  for (i=0; i<nrecv; i++) {
    PASSERTP(ctrl, relmnts[i].key>=0 && relmnts[i].key<npes, 
            (ctrl, "%"PRIDX" %"PRIDX"\n", i, relmnts[i].key));
    relmnts[i].val = perm[i];
  }

  /* OK, now sent it back */
  /* Issue the receives first */
  for (i=0; i<npes; i++) 
    gkMPI_Irecv((void *)(elmnts+scounts[i]), 2*(scounts[i+1]-scounts[i]), IDX_T, 
        i, 1, ctrl->comm, ctrl->rreq+i);

  /* Issue the sends next */
  for (i=0; i<npes; i++) 
    gkMPI_Isend((void *)(relmnts+rcounts[i]), 2*(rcounts[i+1]-rcounts[i]), IDX_T, 
        i, 1, ctrl->comm, ctrl->sreq+i);

  gkMPI_Waitall(npes, ctrl->rreq, ctrl->statuses);
  gkMPI_Waitall(npes, ctrl->sreq, ctrl->statuses);


  /* Construct a partition for the graph */
  graph->where = imalloc(graph->nvtxs+graph->nrecv, "PartSort: graph->where");
  firstvtx = vtxdist[mype];
  for (i=0; i<nvtxs; i++) {
    PASSERTP(ctrl, elmnts[i].key>=0 && elmnts[i].key<npes, 
        (ctrl, "%"PRIDX" %"PRIDX"\n", i, elmnts[i].key));
    PASSERTP(ctrl, elmnts[i].val>=vtxdist[mype] && elmnts[i].val<vtxdist[mype+1], 
        (ctrl, "%"PRIDX" %"PRIDX" %"PRIDX" %"PRIDX"\n", i, vtxdist[mype], vtxdist[mype+1], elmnts[i].val));
    graph->where[elmnts[i].val-firstvtx] = elmnts[i].key;
  }

  WCOREPOP;
}


/**************************************************************************/
/*! This function sorts a distributed list of ikv_t in increasing 
    order, and uses it to compute a partition. It uses a 
    samplesort variant whose number of local samples can potentially
    be smaller than npes. 
*/
/**************************************************************************/
void PseudoSampleSort(ctrl_t *ctrl, graph_t *graph, ikv_t *elmnts)
{
  idx_t npes=ctrl->npes, mype=ctrl->mype; 
  idx_t i, j, k, nlsamples, nvtxs, nrecv, firstvtx, lastvtx;
  idx_t *scounts, *rcounts, *vtxdist, *perm;
  ikv_t *relmnts, *mypicks, *allpicks;

  WCOREPUSH;

  CommUpdateNnbrs(ctrl, npes);

  nvtxs   = graph->nvtxs;
  vtxdist = graph->vtxdist;

  /* determine the number of local samples */
  nlsamples = (GlobalSESum(ctrl, graph->nedges) + graph->gnvtxs)/(npes*npes);
  if (nlsamples > npes)
    nlsamples = npes;
  else if (nlsamples < 100)
    nlsamples = 100;

  if (nlsamples*npes > graph->gnvtxs)
    nlsamples = 5 + graph->gnvtxs/(5*npes);

  IFSET(ctrl->dbglvl, DBG_INFO, 
      rprintf(ctrl, "PseudoSampleSort: nlsamples=%"PRIDX" of %"PRIDX"\n", nlsamples, npes));

  /* get memory for the counts */
  scounts = iwspacemalloc(ctrl, npes+1);
  rcounts = iwspacemalloc(ctrl, npes+1);

  /* get memory for the splitters */
  mypicks  = ikvwspacemalloc(ctrl, npes+1);

  WCOREPUSH; /* for freeing allpicks */
  allpicks = ikvwspacemalloc(ctrl, npes*nlsamples);

  /* Sort the local elements */
  ikvsorti(nvtxs, elmnts);

  /* Select the local nlsamples-1 equally spaced elements */
  for (i=1; i<nlsamples; i++) { 
    mypicks[i-1].key = elmnts[i*nvtxs/nlsamples].key;
    mypicks[i-1].val = elmnts[i*nvtxs/nlsamples].val;
  }

  /* PrintPairs(ctrl, nlsamples-1, mypicks, "Mypicks"); */

  /* Gather the picks to all the processors */
  gkMPI_Allgather((void *)mypicks, 2*(nlsamples-1), IDX_T, (void *)allpicks, 
      2*(nlsamples-1), IDX_T, ctrl->comm);

  /* PrintPairs(ctrl, npes*(nlsamples-1), allpicks, "Allpicks"); */

  /* Sort all the picks */
  ikvsortii(npes*(nlsamples-1), allpicks);

  /* PrintPairs(ctrl, npes*(nlsamples-1), allpicks, "Allpicks"); */

  /* Select the final splitters. Set the boundaries to simplify coding */
  for (i=1; i<npes; i++)
    mypicks[i] = allpicks[i*(nlsamples-1)];
  mypicks[0].key    = IDX_MIN;
  mypicks[npes].key = IDX_MAX;

  /* PrintPairs(ctrl, npes+1, mypicks, "Mypicks"); */

  WCOREPOP;  /* free allpicks */

  /* Compute the number of elements that belong to each bucket */
  iset(npes, 0, scounts);
  for (j=i=0; i<nvtxs; i++) {
    if (elmnts[i].key < mypicks[j+1].key || 
        (elmnts[i].key == mypicks[j+1].key && elmnts[i].val < mypicks[j+1].val))
      scounts[j]++;
    else
      scounts[++j]++;
  }
  gkMPI_Alltoall(scounts, 1, IDX_T, rcounts, 1, IDX_T, ctrl->comm);

  MAKECSR(i, npes, scounts);
  MAKECSR(i, npes, rcounts);

/*
  PrintVector(ctrl, npes+1, 0, scounts, "Scounts");
  PrintVector(ctrl, npes+1, 0, rcounts, "Rcounts");
*/

  /* Allocate memory for sorted elements and receive them */
  nrecv   = rcounts[npes];
  relmnts = ikvwspacemalloc(ctrl, nrecv);

  /* Issue the receives first */
  for (i=0; i<npes; i++) 
    gkMPI_Irecv((void *)(relmnts+rcounts[i]), 2*(rcounts[i+1]-rcounts[i]), 
        IDX_T, i, 1, ctrl->comm, ctrl->rreq+i);

  /* Issue the sends next */
  for (i=0; i<npes; i++) 
    gkMPI_Isend((void *)(elmnts+scounts[i]), 2*(scounts[i+1]-scounts[i]), 
        IDX_T, i, 1, ctrl->comm, ctrl->sreq+i);

  gkMPI_Waitall(npes, ctrl->rreq, ctrl->statuses);
  gkMPI_Waitall(npes, ctrl->sreq, ctrl->statuses);


  /* OK, now do the local sort of the relmnts. Use perm to keep track original order */
  perm = iwspacemalloc(ctrl, nrecv);
  for (i=0; i<nrecv; i++) {
    perm[i]        = relmnts[i].val;
    relmnts[i].val = i;
  }
  ikvsorti(nrecv, relmnts);


  /* Compute what needs to be shifted */
  gkMPI_Scan((void *)(&nrecv), (void *)(&lastvtx), 1, IDX_T, MPI_SUM, ctrl->comm);
  firstvtx = lastvtx-nrecv;  

  /*myprintf(ctrl, "first, last: %"PRIDX" %"PRIDX"\n", firstvtx, lastvtx); */

  for (j=0, i=0; i<npes; i++) {
    if (vtxdist[i+1] > firstvtx) {  /* Found the first PE that is passed me */
      if (vtxdist[i+1] >= lastvtx) {
        /* myprintf(ctrl, "Shifting %"PRIDX" elements to processor %"PRIDX"\n", lastvtx-firstvtx, i); */
        for (k=0; k<lastvtx-firstvtx; k++, j++) 
          relmnts[relmnts[j].val].key = i;
      }
      else {
        /* myprintf(ctrl, "Shifting %"PRIDX" elements to processor %"PRIDX"\n", vtxdist[i+1]-firstvtx, i); */
        for (k=0; k<vtxdist[i+1]-firstvtx; k++, j++) 
          relmnts[relmnts[j].val].key = i;

        firstvtx = vtxdist[i+1];
      }
    }
    if (vtxdist[i+1] >= lastvtx)
      break;
  }

  /* Reverse the ordering on the relmnts[].val */
  for (i=0; i<nrecv; i++) {
    PASSERTP(ctrl, relmnts[i].key>=0 && relmnts[i].key<npes, 
            (ctrl, "%"PRIDX" %"PRIDX"\n", i, relmnts[i].key));
    relmnts[i].val = perm[i];
  }

  /* OK, now sent it back */
  /* Issue the receives first */
  for (i=0; i<npes; i++) 
    gkMPI_Irecv((void *)(elmnts+scounts[i]), 2*(scounts[i+1]-scounts[i]), IDX_T, 
        i, 1, ctrl->comm, ctrl->rreq+i);

  /* Issue the sends next */
  for (i=0; i<npes; i++) 
    gkMPI_Isend((void *)(relmnts+rcounts[i]), 2*(rcounts[i+1]-rcounts[i]), IDX_T, 
        i, 1, ctrl->comm, ctrl->sreq+i);

  gkMPI_Waitall(npes, ctrl->rreq, ctrl->statuses);
  gkMPI_Waitall(npes, ctrl->sreq, ctrl->statuses);


  /* Construct a partition for the graph */
  graph->where = imalloc(graph->nvtxs+graph->nrecv, "PartSort: graph->where");
  firstvtx = vtxdist[mype];
  for (i=0; i<nvtxs; i++) {
    PASSERTP(ctrl, elmnts[i].key>=0 && elmnts[i].key<npes, 
        (ctrl, "%"PRIDX" %"PRIDX"\n", i, elmnts[i].key));
    PASSERTP(ctrl, elmnts[i].val>=vtxdist[mype] && elmnts[i].val<vtxdist[mype+1], 
        (ctrl, "%"PRIDX" %"PRIDX" %"PRIDX" %"PRIDX"\n", i, vtxdist[mype], vtxdist[mype+1], elmnts[i].val));
    graph->where[elmnts[i].val-firstvtx] = elmnts[i].key;
  }

  WCOREPOP;
}

