// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <code@koch.ninja>

#include "precompiled_header_cpp.hpp"

#include <stdlib.h> // free
#include <stddef.h> // size_t, ptrdiff_t

#include "common_cpp.hpp" // INLINE_RELEASE_UNTEMPLATED
#include "bridge_cpp.hpp" // GetCountScores

#include "ebm_internal.hpp" // AddPositiveFloatsSafeBig

#include "dataset_shared.hpp" // SharedStorageDataType
#include "DataSetInteraction.hpp"

namespace DEFINED_ZONE_NAME {
#ifndef DEFINED_ZONE_NAME
#error DEFINED_ZONE_NAME must be defined
#endif // DEFINED_ZONE_NAME

extern ErrorEbm ExtractWeights(
   const unsigned char * const pDataSetShared,
   const BagEbm direction,
   const BagEbm * const aBag,
   const size_t cSetSamples,
   FloatFast ** ppWeightsOut
);

INLINE_RELEASE_UNTEMPLATED static ErrorEbm ConstructGradientsAndHessians(
   const ptrdiff_t cClasses,
   const bool bAllocateHessians,
   const size_t cSetSamples,
   FloatFast ** paGradientsAndHessiansOut
) {
   LOG_0(Trace_Info, "Entered ConstructGradientsAndHessians");

   // cClasses can only be zero if there are zero samples and we shouldn't get here
   EBM_ASSERT(0 != cClasses);
   EBM_ASSERT(1 != cClasses);
   EBM_ASSERT(1 <= cSetSamples);
   EBM_ASSERT(nullptr != paGradientsAndHessiansOut);
   EBM_ASSERT(nullptr == *paGradientsAndHessiansOut);

   const size_t cScores = GetCountScores(cClasses);
   EBM_ASSERT(1 <= cScores);

   const size_t cStorageItems = bAllocateHessians ? size_t { 2 } : size_t { 1 };
   if(IsMultiplyError(sizeof(FloatFast), cScores, cStorageItems, cSetSamples)) {
      LOG_0(Trace_Warning, "WARNING ConstructGradientsAndHessians IsMultiplyError(sizeof(FloatFast), cScores, cStorageItems, cSamples)");
      return Error_OutOfMemory;
   }
   const size_t cBytesGradientsAndHessians = sizeof(FloatFast) * cScores * cStorageItems * cSetSamples;
   ANALYSIS_ASSERT(0 != cBytesGradientsAndHessians);

   FloatFast * const aGradientsAndHessians = static_cast<FloatFast *>(malloc(cBytesGradientsAndHessians));
   if(UNLIKELY(nullptr == aGradientsAndHessians)) {
      LOG_0(Trace_Warning, "WARNING ConstructGradientsAndHessians nullptr == aGradientsAndHessians");
      return Error_OutOfMemory;
   }
   *paGradientsAndHessiansOut = aGradientsAndHessians; // transfer ownership for future deletion

   LOG_0(Trace_Info, "Exited ConstructGradientsAndHessians");
   return Error_None;
}

INLINE_RELEASE_UNTEMPLATED static StorageDataType * * ConstructInputData(
   const unsigned char * const pDataSetShared,
   const BagEbm * const aBag,
   const size_t cSetSamples,
   const size_t cFeatures
) {
   LOG_0(Trace_Info, "Entered DataSetInteraction::ConstructInputData");

   EBM_ASSERT(nullptr != pDataSetShared);
   EBM_ASSERT(1 <= cSetSamples);
   EBM_ASSERT(1 <= cFeatures);

   if(IsMultiplyError(sizeof(StorageDataType), cSetSamples)) {
      // this is to check the allocation inside the loop below
      LOG_0(Trace_Warning, "WARNING DataSetInteraction::ConstructInputData IsMultiplyError(sizeof(StorageDataType), cSetSamples)");
      return nullptr;
   }

   if(IsMultiplyError(sizeof(StorageDataType *), cFeatures)) {
      LOG_0(Trace_Warning, "WARNING DataSetInteraction::ConstructInputData IsMultiplyError(sizeof(StorageDataType *), cFeatures)");
      return nullptr;
   }
   StorageDataType ** const aaInputDataTo = static_cast<StorageDataType **>(malloc(sizeof(StorageDataType *) * cFeatures));
   if(nullptr == aaInputDataTo) {
      LOG_0(Trace_Warning, "WARNING DataSetInteraction::ConstructInputData nullptr == aaInputDataTo");
      return nullptr;
   }

   size_t iFeature = 0;
   StorageDataType ** paInputDataTo = aaInputDataTo;
   do {
      // NOTE: we check this multiplication above just once and not in the loop
      StorageDataType * pInputDataTo = static_cast<StorageDataType *>(malloc(sizeof(StorageDataType) * cSetSamples));
      if(nullptr == pInputDataTo) {
         LOG_0(Trace_Warning, "WARNING DataSetInteraction::ConstructInputData nullptr == pInputDataTo");
         goto free_all;
      }
      *paInputDataTo = pInputDataTo;
      ++paInputDataTo;

      size_t cBins;
      bool bMissing;
      bool bUnknown;
      bool bNominal;
      bool bSparse;
      SharedStorageDataType defaultValSparse;
      size_t cNonDefaultsSparse;
      const void * aInputDataFrom = GetDataSetSharedFeature(
         pDataSetShared,
         iFeature,
         &cBins,
         &bMissing,
         &bUnknown,
         &bNominal,
         &bSparse,
         &defaultValSparse,
         &cNonDefaultsSparse
      );
      EBM_ASSERT(nullptr != aInputDataFrom);
      EBM_ASSERT(!bSparse); // we don't support sparse yet
      EBM_ASSERT(1 <= cBins); // we have samples, and cBins can only be 0 if there are 0 samples

      if(IsConvertError<StorageDataType>(cBins - 1)) {
         // if we check this here, we can be guaranteed that any inputData will convert to StorageDataType
         // since the shared datastructure would not allow data items equal or greater than cBins
         LOG_0(Trace_Error, "ERROR DataSetInteraction::ConstructInputData IsConvertError<StorageDataType>(cBins - 1)");
         goto free_all;
      }

      const BagEbm * pSampleReplication = aBag;

      const SharedStorageDataType * pInputDataFrom = static_cast<const SharedStorageDataType *>(aInputDataFrom);
      const StorageDataType * pInputDataToEnd = &pInputDataTo[cSetSamples];
      do {
         BagEbm replication = 1;
         if(nullptr != pSampleReplication) {
            const BagEbm * pSampleReplicationOriginal = pSampleReplication;
            do {
               replication = *pSampleReplication;
               ++pSampleReplication;
            } while(replication <= BagEbm { 0 });
            const size_t cAdvances = pSampleReplication - pSampleReplicationOriginal - 1;
            pInputDataFrom += cAdvances;
         }
         EBM_ASSERT(0 < replication);

         const SharedStorageDataType inputData = *pInputDataFrom;
         ++pInputDataFrom;

         EBM_ASSERT(!IsConvertError<size_t>(inputData));
         const StorageDataType iData = static_cast<StorageDataType>(inputData);
         EBM_ASSERT(iData < cBins); // enforced by shared data creator
         EBM_ASSERT(!IsConvertError<StorageDataType>(iData)); // since it is smaller than cBins this is guaranteed

         do {
            *pInputDataTo = iData;
            ++pInputDataTo;

            --replication;
         } while(BagEbm { 0 } != replication);
      } while(pInputDataToEnd != pInputDataTo);
      ++iFeature;
   } while(cFeatures != iFeature);

   LOG_0(Trace_Info, "Exited DataSetInteraction::ConstructInputData");
   return aaInputDataTo;

free_all:
   while(aaInputDataTo != paInputDataTo) {
      --paInputDataTo;
      free(*paInputDataTo);
   }
   free(aaInputDataTo);
   return nullptr;
}

void DataSetInteraction::Destruct() {
   LOG_0(Trace_Info, "Entered DataSetInteraction::Destruct");

   free(m_aGradientsAndHessians);
   free(m_aWeights);
   if(nullptr != m_aaInputData) {
      EBM_ASSERT(1 <= m_cFeatures);
      StorageDataType ** paInputData = m_aaInputData;
      const StorageDataType * const * const paInputDataEnd = m_aaInputData + m_cFeatures;
      do {
         EBM_ASSERT(nullptr != *paInputData);
         free(*paInputData);
         ++paInputData;
      } while(paInputDataEnd != paInputData);
      free(m_aaInputData);
   }

   LOG_0(Trace_Info, "Exited DataSetInteraction::Destruct");
}

ErrorEbm DataSetInteraction::Initialize(
   const bool bAllocateGradients,
   const bool bAllocateHessians,
   const unsigned char * const pDataSetShared,
   const BagEbm * const aBag,
   const size_t cSetSamples,
   const size_t cWeights,
   const size_t cFeatures
) {
   EBM_ASSERT(nullptr != pDataSetShared);

   EBM_ASSERT(nullptr == m_aGradientsAndHessians); // we expect to start with zeroed values
   EBM_ASSERT(nullptr == m_aaInputData); // we expect to start with zeroed values
   EBM_ASSERT(0 == m_cSamples); // we expect to start with zeroed values

   LOG_0(Trace_Info, "Entered DataSetInteraction::Initialize");

   ErrorEbm error;

   if(0 != cSetSamples) {
      // if cSamples is zero, then we don't need to allocate anything since we won't use them anyways

      EBM_ASSERT(nullptr == m_aWeights);
      m_weightTotal = static_cast<FloatBig>(cSetSamples);
      if(0 != cWeights) {
         error = ExtractWeights(
            pDataSetShared,
            BagEbm { 1 },
            aBag,
            cSetSamples,
            &m_aWeights
         );
         if(Error_None != error) {
            // error already logged
            return error;
         }
         if(nullptr != m_aWeights) {
            const FloatBig total = AddPositiveFloatsSafeBig(cSetSamples, m_aWeights);
            if(std::isnan(total) || std::isinf(total) || total <= FloatBig { 0 }) {
               LOG_0(Trace_Warning, "WARNING DataSetInteraction::Initialize std::isnan(total) || std::isinf(total) || total <= 0");
               return Error_UserParamVal;
            }
            // if they were all zero then we'd ignore the weights param.  If there are negative numbers it might add
            // to zero though so check it after checking for negative
            EBM_ASSERT(0 != total);
            m_weightTotal = total;
         }
      }

      if(bAllocateGradients) {
         ptrdiff_t cClasses;
         GetDataSetSharedTarget(pDataSetShared, 0, &cClasses);

         // if there are 0 or 1 classes, then with reduction there should be zero scores and the caller should disable
         EBM_ASSERT(0 != cClasses);
         EBM_ASSERT(1 != cClasses);

         error = ConstructGradientsAndHessians(
            cClasses,
            bAllocateHessians,
            cSetSamples,
            &m_aGradientsAndHessians
         );
         if(Error_None != error) {
            // we should have already logged the failure
            return error;
         }
      } else {
         EBM_ASSERT(!bAllocateHessians);
      }

      if(0 != cFeatures) {
         StorageDataType ** const aaInputData = ConstructInputData(
            pDataSetShared,
            aBag,
            cSetSamples,
            cFeatures
         );
         if(nullptr == aaInputData) {
            return Error_OutOfMemory;
         }
         m_aaInputData = aaInputData;
         m_cFeatures = cFeatures; // only needed if nullptr != m_aaInputData
      }
      m_cSamples = cSetSamples;
   }

   LOG_0(Trace_Info, "Exited DataSetInteraction::Initialize");
   return Error_None;
}

} // DEFINED_ZONE_NAME
