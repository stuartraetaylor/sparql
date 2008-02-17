<?php
// $Id$
/**
 * sparql.admin.inc - SPARQL API admin page callbacks.
 *
 * @author Arto Bendiken <http://bendiken.net/>
 * @copyright Copyright (c) 2007-2008 Arto Bendiken. All rights reserved.
 * @license GPL <http://creativecommons.org/licenses/GPL/2.0/>
 * @package sparql.module
 */

//////////////////////////////////////////////////////////////////////////////
// SPARQL settings form

function sparql_admin_settings() {
  $form = array();

  return system_settings_form($form);
}
