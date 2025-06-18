/*******************************************************************************
 * Copyright (c) 2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * uri.c -- URI related operations
 */

#include <uri.h>
#include <stdlib.h>

size_t query_string_length = 0;
char* query_string = NULL;
bool query_string_dirty = false;

/* TODO: Handle key duplicates with a hashmap if required */
query_string_parameters_t query_string_parameters = {0, NULL};

/*-- query_string_add_parameters -----------------------------------------------
 *
 *      Appends the given array of key_value_t elements into global storage.
 *      The fields of each key_value_t instance are shallow copied. If duplicate
 *      keys are seen in two key_value_t instances in the array parameters
 *      both of these instances are appended.
 *
 * Parameters
 *      IN length: the number of elements in the array parameters
 *      IN parameters: an array of key_value_t instances, must be non-NULL
 *
 * Results
 *      ERR_SUCCESS on success or a generic error otherwise.
 *----------------------------------------------------------------------------*/
int query_string_add_parameters(const size_t length,
                                const key_value_t* parameters)
{
   size_t local_length = query_string_parameters.length + length;
   key_value_t *local_parameters = malloc(
      local_length * sizeof(local_parameters[0]));

   if (local_parameters == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   for (size_t index = 0; index < query_string_parameters.length; index++) {
      local_parameters[index] = query_string_parameters.parameters[index];
   }

   for (size_t index = 0; index < length; index++) {
      local_parameters[query_string_parameters.length + index] =
         parameters[index];
   }

   free(query_string_parameters.parameters);

   query_string_parameters.parameters = local_parameters;
   query_string_parameters.length = local_length;
   query_string_dirty = true;

   return ERR_SUCCESS;
}

static const char hexDigits[] = "0123456789abcdef";
static const char uriComponentAllowedChars[] = "-_.!~*'()";

/*-- uriencode -----------------------------------------------------------------
 *
 *      This function follows RFC-3986 and implements URL encoding.
 *      It runs in one of two ways - it can edit nothing and return the length
 *      of the URL encoded text which callers can use to allocate a buffer with
 *      the correct size, or it can create the URL encoded result in-place in a
 *      user specified buffer which must be long enough to hold the result.
 *
 * Parameters
 *      IN input: The C-string to encode, can be NULL
 *      IN output_buffer_length: The length of the buffer where the result is
 *         constructed including the terminating NULL character, unused if
 *         output_buffer is NULL
 *      OUT output_buffer: If NULL, the function retruns the length of the
 *         result. Otherwise, this parameter is filled with the resulting
 *         URL encoded text. It needs to be large enough as written in the
 *         parameter output_buffer_length.
 *      IN offset: If output_buffer is non-NULL, offset is the index in
 *         output_buffer where the result in-place creting will start from.
 *      IN do_not_encode: If true then every character in the parameter input
 *         will be encoded as itself, otherwise the special characters will be
 *         encoded with their ascii code written in base 16 in lower case
 *         prefixed by the '%' character.
 *
 *
 * Results
 *      The length of the encoded result if do_not_encode is false, the length
 *         of input otherwise. If input is NULL 0 is returned.
 *----------------------------------------------------------------------------*/
size_t uriencode(
   const char *input,
   const size_t output_buffer_length,
   char * const output_buffer,
   const size_t offset,
   const bool do_not_encode)
{
   size_t current_output_length = 0;

   if (input == NULL) {
      return 0;
   }

   for (size_t i = 0; input[i] != '\0'; ++i) {
      if (do_not_encode || isalnum(input[i]) || strchr(uriComponentAllowedChars,
         input[i]) != NULL) {
         if (output_buffer && offset + current_output_length <
            output_buffer_length) {
            output_buffer[offset + current_output_length] = input[i];
         }
         current_output_length++;
      } else {
         const char elems[] = { '%', hexDigits[input[i] >> 4],
            hexDigits[input[i] & 0x0F] };
         for (size_t j = 0; j < sizeof(elems); ++j) {
            if (output_buffer && offset + current_output_length <
               output_buffer_length) {
               output_buffer[offset + current_output_length] = elems[j];
            }
            current_output_length++;
         }
      }
   }

   return current_output_length;
}

/*-- generate_query_string -----------------------------------------------------
 *
 *      This function creates a query string in-place from the elements added
 *      by the function query_string_add_parameters. When printing each
 *      key_value_t, "=" is placed between the key and the value and "&" is
 *      used as the separator between each key_value_t instance.
 *
 * Parameters
 *      IN length: the length of the string, or unused if string is NULL.
 *      IN string: If NULL, string is unused, otherwise string is
 *         the buffer where the result is printed into.
 *
 * Results
 *      The query string length.
 *----------------------------------------------------------------------------*/
size_t generate_query_string(const size_t length, char* string)
{
   size_t query_string_length = 0;

   const char* EQUALS_STRING = "=";
   const char* AND_STRING = "&";

   if (query_string_parameters.length == 0 ||
       query_string_parameters.parameters == NULL) {
      return 0;
   }

   /*
    * calculate length of all entries concatenated when string is NULL,
    * otherwise concatenate by filling the buffer string
    */

   if (query_string_parameters.length > 0 ) {
      /* key_str=value_str for the first */
      query_string_length += uriencode(
         query_string_parameters.parameters[0].key,
         length,
         string,
         query_string_length,
         false
      );

      query_string_length += uriencode(
         EQUALS_STRING,
         length,
         string,
         query_string_length,
         true
      );

      query_string_length += uriencode(
         query_string_parameters.parameters[0].value,
         length,
         string,
         query_string_length,
         false
      );

      /* &key_str=value_str for the rest */
      for (size_t i = 1; i < query_string_parameters.length; ++i) {
         query_string_length += uriencode(
            AND_STRING,
            length,
            string,
            query_string_length,
            true
         );

         query_string_length += uriencode(
            query_string_parameters.parameters[i].key,
            length,
            string,
            query_string_length,
            false
         );

         query_string_length += uriencode(
            EQUALS_STRING,
            length,
            string,
            query_string_length,
            true
         );

         query_string_length += uriencode(
            query_string_parameters.parameters[i].value,
            length,
            string,
            query_string_length,
            false
         );
      }
   }

   return query_string_length;
}

/*-- regenerate_query_string ---------------------------------------------------
 *
 *      This function is used to save the entries from query_string_parameters
 *      into a C-string globally. Repetitive calls of this function will not
 *      have any effect until new entries are added by calling the function
 *      query_string_add_parameters.
 *
 * Parameters
 *
 * Results
 *      ERR_SUCCESS on success or a generic error status otherwise.
 *----------------------------------------------------------------------------*/
int regenerate_query_string(void)
{
   size_t length = generate_query_string(0, NULL);
   char* string = malloc((length + 1) * sizeof(string[0]));

   if (string == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   length = generate_query_string(length, string);
   string[length] = '\0';

   free(query_string);
   query_string = string;
   query_string_length = length;

   return ERR_SUCCESS;
}

/*-- query_string_get ----------------------------------------------------------
 *
 *      This function can be used to access the query string. It uses a caching
 *      mechanism to avoid repetitive concatenation.
 *
 * Parameters
 *
 *      OUT query_string_ptr: The out pointer where a shallow copy is returned
 *         or NULL if there aren't any query strings.
 * Results
 *      ERR_SUCCESS on success or a generic error status otherwise.
 *----------------------------------------------------------------------------*/
int query_string_get(const char** query_string_ptr)
{
   int result = ERR_SUCCESS;
   if (query_string_dirty) {
      if ((result = regenerate_query_string()) != ERR_SUCCESS) {
         return result;
      }
      query_string_dirty = false;
   }

   *query_string_ptr = query_string;

   return ERR_SUCCESS;
}

/*-- query_string_cleanup ------------------------------------------------------
 *
 *      This function cleans up all of the global data from bootlib/uri.c
 *
 * Parameters
 *
 * Results
 *
 *----------------------------------------------------------------------------*/
void query_string_cleanup(void) {
   free(query_string);
   query_string = NULL;
   query_string_dirty = false;

   free(query_string_parameters.parameters);
   query_string_parameters.parameters = NULL;
   query_string_parameters.length = 0;
}
