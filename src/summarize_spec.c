#include "search_options.h"

#include "rmutil/util.h"
#include "rmutil/strings.h"
#include "rmutil/args.h"
#include "util/array.h"

///////////////////////////////////////////////////////////////////////////////////////////////

/**
 * HIGHLIGHT [FIELDS {num} {field}…] [TAGS {open} {close}]
 * SUMMARISE [FIELDS {num} {field} …] [LEN {len}] [FRAGS {num}]
 */

int FieldList::parseFieldList(ArgsCursor *ac, Array *fieldPtrs) {
  ArgsCursor fieldArgs = {0};
  if (AC_GetVarArgs(ac, &fieldArgs) != AC_OK) {
    return -1;
  }

  while (!AC_IsAtEnd(&fieldArgs)) {
    const char *name = AC_GetStringNC(&fieldArgs, NULL);
    ReturnedField *fieldInfo = GetCreateField(name);
    size_t ix = fieldInfo - fields;
    Array_Write(fieldPtrs, &ix, sizeof(size_t));
  }

  return 0;
}

//---------------------------------------------------------------------------------------------

void HighlightSettings::setHighlightSettings(const HighlightSettings *defaults) {
  rm_free(closeTag);
  rm_free(openTag);

  closeTag = NULL;
  openTag = NULL;
  if (defaults->openTag) {
    openTag = rm_strdup(defaults->openTag);
  }
  if (defaults->closeTag) {
    closeTag = rm_strdup(defaults->closeTag);
  }
}

//---------------------------------------------------------------------------------------------

void SummarizeSettings::setSummarizeSettings(const SummarizeSettings *defaults) {
  *this = *defaults;
  if (separator) {
    separator = rm_strdup(separator);
  }
}

//---------------------------------------------------------------------------------------------

void ReturnedField::setFieldSettings(const ReturnedField *defaults, int isHighlight) {
  if (isHighlight) {
    highlightSettings.setHighlightSettings(&defaults->highlightSettings);
    mode |= SummarizeMode_Highlight;
  } else {
    summarizeSettings.setSummarizeSettings(&defaults->summarizeSettings);
    mode |= SummarizeMode_Synopsis;
  }
}

//---------------------------------------------------------------------------------------------

int FieldList::parseArgs(ArgsCursor *ac, bool isHighlight) {
  size_t numFields = 0;
  int rc = REDISMODULE_OK;

  ReturnedField defOpts;

  Array fieldPtrs;
  Array_Init(&fieldPtrs);

  if (AC_AdvanceIfMatch(ac, "FIELDS")) {
    if (parseFieldList(ac, fields, &fieldPtrs) != 0) {
      rc = REDISMODULE_ERR;
      goto done;
    }
  }

  while (!AC_IsAtEnd(ac)) {
    if (isHighlight && AC_AdvanceIfMatch(ac, "TAGS")) {
      // Open tag, close tag
      if (AC_NumRemaining(ac) < 2) {
        rc = REDISMODULE_ERR;
        goto done;
      }
      defOpts.highlightSettings.openTag = (char *)AC_GetStringNC(ac, NULL);
      defOpts.highlightSettings.closeTag = (char *)AC_GetStringNC(ac, NULL);
    } else if (!isHighlight && AC_AdvanceIfMatch(ac, "LEN")) {
      if (AC_GetUnsigned(ac, &defOpts.summarizeSettings.contextLen, 0) != AC_OK) {
        rc = REDISMODULE_ERR;
        goto done;
      }
    } else if (!isHighlight && AC_AdvanceIfMatch(ac, "FRAGS")) {
      unsigned tmp;
      if (AC_GetUnsigned(ac, &tmp, 0) != AC_OK) {
        rc = REDISMODULE_ERR;
        goto done;
      }
      defOpts.summarizeSettings.numFrags = tmp;
    } else if (!isHighlight && AC_AdvanceIfMatch(ac, "SEPARATOR")) {
      if (AC_GetString(ac, (const char **)&defOpts.summarizeSettings.separator, NULL, 0) != AC_OK) {
        rc = REDISMODULE_ERR;
        goto done;
      }
    } else {
      break;
    }
  }

  if (fieldPtrs.len) {
    size_t numNewPtrs = ARRAY_GETSIZE_AS(&fieldPtrs, size_t);
    for (size_t ii = 0; ii < numNewPtrs; ++ii) {
      size_t ix = ARRAY_GETARRAY_AS(&fieldPtrs, size_t *)[ii];
      ReturnedField *fieldInfo = &fields->fields[ix];
      fieldInfo->setFieldSettings(&defOpts, isHighlight);
    }
  } else {
    fields->defaultField.setFieldSettings(&defOpts, isHighlight);
  }

done:
  Array_Free(&fieldPtrs);
  return rc;
}

//---------------------------------------------------------------------------------------------

void FieldList::ParseSummarize(ArgsCursor *ac) {
  if (parseArgs(ac, false) == REDISMODULE_ERR) {
    throw Error("Bad arguments for SUMMARIZE");
  }
}

//---------------------------------------------------------------------------------------------

void FieldList::ParseHighlight(ArgsCursor *ac) {
  if (parseArgs(ac, true) == REDISMODULE_ERR) {
    throw Error("Bad arguments for HIGHLIGHT");
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
