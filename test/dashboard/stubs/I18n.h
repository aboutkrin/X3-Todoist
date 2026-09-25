#pragma once
#include "I18nKeys.h"
const char* stubText(StrId id);
struct I18nStub {
  const char* get(StrId id) const { return stubText(id); }
};
inline I18nStub I18N;
#define tr(key) stubText(StrId::key)
