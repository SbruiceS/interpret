// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <code@koch.ninja>

#include "PrecompiledHeader.h"
#include <vector>
#include "Loss.h"
#include "Registration.h"

// Steps for adding a new loss/objective function in C++:
//   1) Copy one of the existing Loss*.h include files into a new renamed Loss*.h file
//      (for regression, we recommend starting from LossRegressionPseudoHuber.h)
//   2) Modify the new Loss*.h file to handle the new loss function
//   3) Add [#include "Loss*.h"] to the list of other include files right below this guide
//   4) Add the Loss* type to the list of loss registrations in the RegisterLosses() function right below the includes
//   5) Modify the Register<Loss*>("loss_function_name", ...) entry to have the new loss function name
//      and the list of parameters needed for the loss function which are to be extracted from the loss string.
//   6) Update/verify that the constructor arguments on your Loss* class match the parameters in the loss registration
//      below. If the list of *LossParam items in the function RegisterLosses() do not match your constructor
//      parameters in the new Loss* struct, it will not compile and cryptic compile errors will be produced.
//   5) Recompile the C++ with either build.sh or build.bat depending on your operating system
//   6) Enjoy your new Loss function, and send us a PR on Github if you think others would benefit  :-)

// Add new Loss*.h include files here:
#include "LossBinaryCrossEntropy.h"
#include "LossBinaryLogLoss.h"
#include "LossMulticlassCrossEntropy.h"
#include "LossMulticlassLogLoss.h"
#include "LossMultitaskBinaryLogLoss.h"
#include "LossMultitaskMulticlassCrossEntropy.h"
#include "LossMultitaskRegressionMse.h"
#include "LossRegressionMse.h"
#include "LossRegressionPseudoHuber.h"

// Add new Loss* type registrations to this list:
static const std::vector<std::shared_ptr<const Registration>> RegisterLosses() {
   // IMPORTANT: the *LossParam types here must match the parameters types in your Loss* constructor
   return {
      Register<LossMulticlassLogLoss>("log_loss"),
      Register<LossRegressionPseudoHuber>("pseudo_huber", FloatParam("delta", 1))
   };
}

// !! ANYTHING BELOW THIS POINT ISN'T REQUIRED TO MAKE YOUR OWN CUSTOM LOSS FUNCTION !!

ErrorEbmType Loss::CreateLoss(
   const char * const sLoss,
   const Config * const pConfig,
   const Loss ** const ppLoss
) noexcept {
   EBM_ASSERT(nullptr != sLoss);
   EBM_ASSERT(nullptr != pConfig);
   EBM_ASSERT(nullptr != ppLoss);

   LOG_0(TraceLevelInfo, "Entered Loss::CreateLoss");
   try {
      const std::vector<std::shared_ptr<const Registration>> registrations = RegisterLosses();
      std::unique_ptr<const Registrable> pRegistrable = Registration::CreateRegistrable(registrations, sLoss, pConfig);
      if(nullptr == pRegistrable) {
         LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss loss unknown");
         return Error_LossUnknown;
      }
      *ppLoss = static_cast<const Loss *>(pRegistrable.release());
      LOG_0(TraceLevelInfo, "Exited Loss::CreateLoss");
      return Error_None;
   } catch(const ParameterValueMalformedException &) {
      LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss ParameterValueMalformedException");
      return Error_LossParameterValueMalformed;
   } catch(const ParameterUnknownException &) {
      LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss ParameterUnknownException");
      return Error_LossParameterUnknown;
   } catch(const RegistrationConstructorException &) {
      LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss RegistrationConstructorException");
      return Error_LossConstructorException;
   } catch(const ParameterValueOutOfRangeException &) {
      LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss ParameterValueOutOfRangeException");
      return Error_LossParameterValueOutOfRange;
   } catch(const ParameterMismatchWithConfigException &) {
      LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss ParameterMismatchWithConfigException");
      return Error_LossParameterMismatchWithConfig;
   } catch(const EbmException & exception) {
      LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss EbmException");
      return exception.GetError();
   } catch(const std::bad_alloc &) {
      LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss Out of Memory");
      return Error_OutOfMemory;
   } catch(...) {
      LOG_0(TraceLevelWarning, "WARNING Loss::CreateLoss internal error, unknown exception");
      return Error_UnknownInternalError;
   }
}

FloatEbmType Loss::GetUpdateMultiple() const {
   return FloatEbmType { 1 };
}

bool Loss::IsSuperSuperSpecialLossWhereTargetNotNeededOnlyMseLossQualifies() const {
   return false;
}
