<?php
// $Id$
/**
 * @file 
 * SPARQL HTTP client implementation.
 *
 * @author Arto Bendiken <http://bendiken.net/>
 * @copyright Copyright (c) 2007-2008 Arto Bendiken. All rights reserved.
 * @license GPL <http://creativecommons.org/licenses/GPL/2.0/>
 * @package sparql.module
 */

//////////////////////////////////////////////////////////////////////////////
// SPARQL HTTP client

function sparql_request($endpoint, $query, $options = array(), &$errors = NULL) {
  $options = array_merge(array('query' => $query, 'format' => 'application/sparql-results+json', 'output' => 'json'), $options);

  $request = $endpoint . '?' . http_build_query($options, NULL, '&');
  $headers = array('Accept' => $options['format'] . ', */*');
  if ($response = drupal_http_request($request, $headers, 'GET')) {
    if (empty($response->code) && empty($response->data)) {
      $errors[] = t('Remote SPARQL query timed out; please try again.');
      watchdog("sparql", 'Remote SPARQL query timed out.');
    }
    elseif (isset($response->error) && empty($response->error)) {
      // This is seen when specifying a URL that doesn't even resolve
      $errors[] = t('Invalid SPARQL endpoint specified.');
      watchdog("sparql", 'Invalid SPARQL endpoint specified. %endpoint', array('%endpoint' => $endpoint));
    }
    elseif (isset($response->error)) {
      $error = str_replace(sparql_default_prefixes() . "\n", '', $response->data);
      $errors[] = t('Remote SPARQL endpoint returned error: %error', array('%error' => $error));
      watchdog("sparql", 'Remote SPARQL endpoint returned error: %error', array('%error' => $error));
    }
    elseif (preg_match('!^([\w]+/[^;]+)!', $response->headers['Content-Type'], $matches)) {

      switch ($format = $matches[1]) {
        case 'application/sparql-results+json':
        case 'text/javascript':
          return sparql_parse_json_results($response->data);

        case 'application/sparql-results+xml':
          return sparql_parse_xml_results($response->data);
        case 'text/plain':
          // Text plain could be either.
          if (json_decode($response->data)) {
            return sparql_parse_json_results($response->data);
          }
          elseif (simplexml_load_string($response->data)) {
            return sparql_parse_xml_results($response->data);
          }
          // If either case is not true then drop through to default.
        default:
          $errors[] = t('Unknown SPARQL results format: %type.', array('%type' => $format));
          watchdog("sparql", 'Unknown SPARQL results format: %type.', array('%type' => $format));
          break;
      }
    }
  }

  return FALSE;
}

/**
 * Parses query results in application/sparql-results+json format.
 *
 * @see http://www.w3.org/TR/rdf-sparql-json-res/
 */
function sparql_parse_json_results($input) {
  $input = json_decode($input);

  if (isset($input->boolean)) {
    return (bool)$input->boolean;
  }

  $output = $bnodes = array();
  foreach ($input->results->bindings as $row) {
    $item = array();

    foreach (get_object_vars($row) as $field => $binding) {
      if (isset($binding->type)) {
        switch ($binding->type) {
          case 'uri':
            $value = $binding->value;
            break;
          case 'bnode':
            $bnode_id = $binding->value;
            $value = isset($bnodes[$bnode_id]) ? $bnodes[$bnode_id] : ($bnodes[$bnode_id] = rdf_bnode());
            break;
          case 'literal':
            $value = isset($binding->{'xml:lang'}) ? rdf_literal($binding->value, $binding->{'xml:lang'}) : $binding->value;
            break;
          case 'typed-literal':
            $value = rdf_literal($binding->value, NULL, $binding->datatype);
            break;
          default: // unknown/invalid
            continue 2; // skip this binding
            break;
        }
      }
      else {
        $value = NULL; 
      }
      
      $item[$field] = $value;
    }

    $output[] = $item;
  }
  return $output;
}

/**
 * Parses query results in application/sparql-results+xml format.
 *
 * @see http://www.w3.org/TR/rdf-sparql-XMLres/
 */
function sparql_parse_xml_results($input) {
  // FIXME: we seem to have some encoding issues querying against DBpedia.
  $input = simplexml_load_string($input);

  if (isset($input->boolean)) {
    return (string)$input->boolean == 'true';
  }

  $output = $bnodes = array();
  foreach ($input->results->result as $result) {
    $item = array();

    foreach ($result->binding as $binding) {
      if (isset($binding->uri)) {
        $value    = (string)$binding->uri;
      }
      elseif (isset($binding->bnode)) {
        $bnode_id = (string)$binding->bnode;
        $value    = isset($bnodes[$bnode_id]) ? $bnodes[$bnode_id] : ($bnodes[$bnode_id] = rdf_bnode());
      }
      else {
        $literal  = $binding->literal;
        $datatype = isset($literal['datatype']) ? (string)$literal['datatype'] : NULL;
        $language = _sparql_parse_xml_results_language($literal);
        $value    = rdf_literal((string)$literal, $language, $datatype);
      }

      $item[(string)$binding['name']] = $value;
    }

    $output[] = $item;
  }

  return $output;
}

function _sparql_parse_xml_results_language($element) {
  foreach ($element->attributes('http://www.w3.org/XML/1998/namespace') as $name => $value) {
    if ($name == 'lang') {
      return (string)$value;
    }
  }
}
