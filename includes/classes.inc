<?php

/**
 * @file
 * Implementation of classes for the comparative edition solution pack. 
 */

class IslandoraComparativeEditionObject { 

  public static function formatSentenceUnaligned($sentence_array) {
    $formatted_sentence_array;
    foreach ($sentence_array as $pid => $versions) {
      $page_label = islandora_object_load($pid)->label; 
      $parent_pid = IslandoraComparativeEditionPageObject::getParent($pid);
      $parent_label = islandora_object_load($parent_pid)->label;
      $table_header = "<a class='icesp-table-row' href='/islandora/object/{$pid}'><div class='icesp-table-header'>{$parent_label}, Page {$page_label}</div>";
      foreach ($versions as $version) {
        $formatted_sentence_array[] = array($table_header => "<div class='icesp-unaligned'>{$version}</div></a>");
      }
    }
 
    return $formatted_sentence_array;
  }

  public static function formatSentenceAligned($sentence_array) {
    $collatex_url = variable_get('islandora_comparative_edition_collatex_server_url', 'https://collatex.net/demo/collate');
    $formatted_sentence_array = array();
    
    // Build CollateX request
    $collatex_request = array();
    $collatex_request['algorithm'] = variable_get('islandora_comparative_edition_collatex_alignment_algorithm', 'dekker');

    $all_sentence_versions = array();
    foreach ($sentence_array as $pid => $versions) {
      foreach ($versions as $version) {
        // Add 10 digit random number to differentiate sentences from same PID
        $collatex_request['witnesses'][] = array("id" => $pid . "." . rand(1000000000, 9999999999), "content" => $version);
      }
    }

    $curl = curl_init($collatex_url);
    curl_setopt($curl, CURLOPT_HEADER, false);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
      curl_setopt($curl, CURLOPT_HTTPHEADER, array(
      "Content-type: application/json",
      "Accept: application/json"
    ));
    curl_setopt($curl, CURLOPT_POST, true);
    curl_setopt($curl, CURLOPT_POSTFIELDS, json_encode($collatex_request));
    $collatex_response = json_decode(curl_exec($curl), TRUE);
    $status = curl_getinfo($curl, CURLINFO_HTTP_CODE);
    if ($status != 200) { 
      drupal_set_message("Error connecting to {$collatex_url}, recieved a non-200 response.", 'error');
    }
    curl_close($curl);

    // Format CollateX response
    $formatted_sentence_keys = array();
    foreach ($collatex_response['witnesses'] as $key => $pid) {
      $stripped_pid = preg_replace('/\.\d{10}$/', '', $pid);
      $page_label = islandora_object_load($stripped_pid)->label; 
      $parent_pid = IslandoraComparativeEditionPageObject::getParent($stripped_pid);
      $parent_label = islandora_object_load($parent_pid)->label;
      $table_header = "<a class='icesp-table-row' href='/islandora/object/{$stripped_pid}'><div class='icesp-table-header'>{$parent_label}, Page {$page_label}</div>";
      $formatted_sentence_keys[$key] = $table_header;
    }

    $formatted_sentence_values = array();
    $witness_count = count($formatted_sentence_keys) - 1;
    for ($i = 0; $i <= $witness_count; $i++) {
      $formatted_sentence_values[$i] = "";
    }

    foreach ($collatex_response['table'] as $section) {

      // Determine if section is invariant
      $section_count = count($section) - 1;
      $exemplar = $section[0];
      $variant = FALSE;
      for ($i = 0; $i <= $section_count; $i++) {
        if ($section[$i] !== $exemplar) {
          $variant = TRUE;
          break;
        }
      }

      foreach ($section as $key => $version) {
        $variance = ($variant ? "variant" : "invariant");
        if (count($version) == 0) {
          $variance = "empty";
        }
        $formatted_sentence_values[$key] .= "<div class='icesp-section-{$variance}'>";
        foreach ($version as $token) {
          $formatted_sentence_values[$key] .= $token;
        }
        //Trim final space
        $formatted_sentence_values[$key] = rtrim($formatted_sentence_values[$key], " ");
        $formatted_sentence_values[$key] .= "</div>";
      }
    }

    foreach ($formatted_sentence_values as $key => $value) {
      $formatted_sentence_values[$key] .= "</a>";
    }

    foreach ($formatted_sentence_keys as $key => $value) {
      $formatted_sentence_array[] = array($formatted_sentence_keys[$key] => $formatted_sentence_values[$key]);
    }

    return $formatted_sentence_array;
  }

  public static function generateAnalysisMindex($ce_pid) {

    // Gather grandchild text pindices
    $missing_pindices = array();
    $pindices = array();
    $witnesses = self::getChildren($ce_pid);
    foreach ($witnesses as $witness) {
      $pages = IslandoraComparativeEditionWitnessObject::getChildren($witness);
      foreach ($pages as $page) {
        $page_object = islandora_object_load($page);
        if ($page_object['TEXTPINDEX']) {
          $pindices[$page] = $page_object['TEXTPINDEX']->content;
        }
        else {
          $missing_pindices[] = $page_object->id;
        }
      }
    }

    // Notify user if missing TEXTPINDEX found during reindexing
    if (!empty($missing_pindices)) {
      $missing_pindices_msg = "The following pages are missing TEXTPINDEX datastreams, causing errors while rebuilding {$ce_pid}'s ANALYSISMINDEX:";
      foreach ($missing_pindices as $missing_pindex) {
        $missing_pindices_msg .= "<br/>- {$missing_pindex}";
      }
      drupal_set_message($missing_pindices_msg, 'error');
    }

    // Extract data from pindices to form collation array
    $collation = array();
    foreach ($pindices as $page_pid => $pindex) {
      $lines = explode("\n", trim($pindex));
      foreach ($lines as $line) {
        $sentence = explode(variable_get('islandora_comparative_edition_pindex_delimiter', '|'), $line);
        $sentence_id = array_shift($sentence);
        foreach ($sentence as $part) {
          $collation[$sentence_id][$page_pid][] = $part;
        }
      }
    }
 
    // Format collated sentences into ANALYSISMINDEX
    $mindex = array();
    foreach ($collation as $sentence_id => $sentence_array) {
      $mindex[$sentence_id] = self::formatSentenceAligned($sentence_array);
    }

    // Write data to ANALYSISMINDEX datastream
    $ce_object = islandora_object_load($ce_pid);
    $ce_object->purgeDatastream('ANALYSISMINDEX');
    $mindex_datastream = $ce_object->constructDatastream('ANALYSISMINDEX');
    $mindex_datastream->label = 'Analysis Master Index';
    $mindex_datastream->mimeType = 'application/json'; 
    $mindex_datastream->setContentFromString(json_encode($mindex));
    $ce_object->ingestDatastream($mindex_datastream);
    watchdog("Islandora Comparative Editions", "ANALYSISMINDEX for '{$ce_object->id}' has been rebuilt.");
  }
  
  public static function generateDisplayMindex($ce_pid) {

    // Gather grandchild text pindices
    $missing_pindices = array();
    $pindices = array();
    $witnesses = self::getChildren($ce_pid);
    foreach ($witnesses as $witness) {
      $pages = IslandoraComparativeEditionWitnessObject::getChildren($witness);
      foreach ($pages as $page) {
        $page_object = islandora_object_load($page);
        if ($page_object['HTMLPINDEX']) {
          $pindices[$page] = $page_object['HTMLPINDEX']->content;
        }
        else {
          $missing_pindices[] = $page_object->id;
        }
      }
    }

    // Notify user if missing HTMLPINDEX found during reindexing
    if (!empty($missing_pindices)) {
      $missing_pindices_msg = "The following pages are missing HTMLPINDEX datastreams, causing errors while rebuilding {$ce_pid}'s DISPLAYMINDEX:";
      foreach ($missing_pindices as $missing_pindex) {
        $missing_pindices_msg .= "<br/>- {$missing_pindex}";
      }
      drupal_set_message($missing_pindices_msg, 'error');
    }

    // Extract data from pindices to form collation array
    $collation = array();
    foreach ($pindices as $page_pid => $pindex) {
      $lines = explode("\n", trim($pindex));
      foreach ($lines as $line) {
        $sentence = explode(variable_get('islandora_comparative_edition_pindex_delimiter', '|'), $line);
        $sentence_id = array_shift($sentence);
        foreach ($sentence as $part) {
          $collation[$sentence_id][$page_pid][] = $part;
        }
      }
    }
 
    // Format collated sentences into DISPLAYMINDEX
    $mindex = array();
    foreach ($collation as $sentence_id => $sentence_array) {
      $mindex[$sentence_id] = self::formatSentenceUnaligned($sentence_array);
    }

    // Write data to DISPLAYMINDEX datastream
    $ce_object = islandora_object_load($ce_pid);
    $ce_object->purgeDatastream('DISPLAYMINDEX');
    $mindex_datastream = $ce_object->constructDatastream('DISPLAYMINDEX');
    $mindex_datastream->label = 'Display Master Index';
    $mindex_datastream->mimeType = 'application/json'; 
    $mindex_datastream->setContentFromString(json_encode($mindex));
    $ce_object->ingestDatastream($mindex_datastream);
    watchdog("Islandora Comparative Editions", "DISPLAYMINDEX for '{$ce_object->id}' has been rebuilt.");
  }

  public static function getChildren($pid) {
    $query = <<<EOQ
SELECT ?child ?seq FROM <#ri> WHERE {
  ?child <info:fedora/fedora-system:def/relations-external#isMemberOf> <info:fedora/{$pid}> .
  ?child <http://islandora.ca/ontology/relsext#isSequenceNumber> ?seq .
}
EOQ;
    $connection = islandora_get_tuque_connection();
    $results = $connection->repository->ri->sparqlQuery($query);
    $children = array();
    foreach ($results as $result) {
      $child = $result['child']['value'];
      $seq = $result['seq']['value'];
      $children[$seq] = $child;
    }
    
    return $children;
  }

}

class IslandoraComparativeEditionWitnessObject { 

  public static function recursiveDeleteChildren($pid) {
    $object = islandora_object_load($pid);
    $parent = self::getParent($pid);
    $children = self::getChildren($pid);
    $connection = islandora_get_tuque_connection();
    $repository = $connection->repository;
  
    foreach ($children as $child) {
      watchdog("Islandora Comparative Edition SP", "Deleting child page object {$child} of witness {$pid}");
      $repository->purgeObject($child);
    }
  }

  public static function getParent($pid) {
    $object = islandora_object_load($pid);
    $parent = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOf');    
    return $parent[0]['object']['value'];
  }

  public static function getChildren($pid) {
    $query = <<<EOQ
SELECT ?child ?seq FROM <#ri> WHERE {
  ?child <info:fedora/fedora-system:def/relations-external#isMemberOf> <info:fedora/{$pid}> .
  ?child <http://islandora.ca/ontology/relsext#isSequenceNumber> ?seq .
}
EOQ;
    $connection = islandora_get_tuque_connection();
    $results = $connection->repository->ri->sparqlQuery($query);
    $children = array();
    foreach ($results as $result) {
      $child = $result['child']['value'];
      $seq = $result['seq']['value'];
      $children[$seq] = $child;
    }
    
    return $children;
  }

  public function ingest($parameters, $files) {

    module_load_include('inc', 'islandora', 'includes/utilities');
    $parent_id = $parameters['parent'];
    $parent_namespace = islandora_get_namespace($parent_id);
    $siblings = IslandoraComparativeEditionObject::getChildren($parent_id); 
    $seqnum = count($siblings) + 1;

    $connection = islandora_get_tuque_connection();
    $repository = $connection->repository;
    $witness_object = $repository->constructObject($parent_namespace);
    $witness_object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:comparativeEditionWitnessCModel');
    $witness_object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $parent_id);
    $witness_object->relationships->add(ISLANDORA_RELS_EXT_URI, 'isSequenceNumber', $seqnum, RELS_TYPE_PLAIN_LITERAL);

    // Construct MODS datastream
    $mods_datastream = $witness_object->constructDatastream('MODS');
    $mods_datastream->label = 'MODS';
    $mods_datastream->mimeType = 'application/xml';
    $mods_datastream->setContentFromFile($files['mods']);
    $witness_object->ingestDatastream($mods_datastream);
    $witness_object->label = simplexml_load_file($files['mods'])->titleInfo->partName;

    // Construct OBJ datastream
    $obj_datastream = $witness_object->constructDatastream('OBJ');
    $obj_datastream->label = 'OBJ';
    $obj_datastream->mimeType = mime_content_type($files['obj']); 
    $obj_datastream->setContentFromFile($files['obj']);
    $witness_object->ingestDatastream($obj_datastream);

    // Construct DISPLAY-CSS datastream
    $css_datastream = $witness_object->constructDatastream('DISPLAY-CSS');
    $css_datastream->label = 'DISPLAY-CSS';
    $css_datastream->mimeType = 'text/css'; 
    $css_datastream->setContentFromFile($files['css']);
    $witness_object->ingestDatastream($css_datastream);

    $witness_id = $repository->ingestObject($witness_object)->id;

    // Construct page objects
    foreach ($files['pages'] as $page) {
      $page_object = new IslandoraComparativeEditionPageObject();
      $page_object->ingest($page, $witness_id);
    }

    // Trigger rebuilding of parent ANALYSISMINDEX and DISPLAYMINDEX
    IslandoraComparativeEditionObject::generateAnalysisMindex($parent_id);
    IslandoraComparativeEditionObject::generateDisplayMindex($parent_id);
  }

}

class IslandoraComparativeEditionPageObject { 

  public static function getParent($pid) {
    $object = islandora_object_load($pid);
    $parent = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOf');    
    return $parent[0]['object']['value'];
  }

  public static function getGrandparent($pid) {
    $parent = self::getParent($pid);
    $grandparent = IslandoraComparativeEditionWitnessObject::getParent($parent);
    return $grandparent; 
  }

  public static function getSiblings($pid) {
    $parent = self::getParent($pid);
    $query = <<<EOQ
SELECT ?child ?seq FROM <#ri> WHERE {
  ?child <info:fedora/fedora-system:def/relations-external#isMemberOf> <info:fedora/{$parent}> .
  ?child <http://islandora.ca/ontology/relsext#isSequenceNumber> ?seq .
}
EOQ;
    $connection = islandora_get_tuque_connection();
    $results = $connection->repository->ri->sparqlQuery($query);
    $children = array();
    foreach ($results as $result) {
      $child = $result['child']['value'];
      $seq = $result['seq']['value'];
      $label = islandora_object_load($result['child']['value'])->label;
      $children[$child] = array('pid' => $child, 'label' => $label, 'seq' => $seq);
    }
    
    foreach ($children as $key => $row) {
      $sequence[$key] = $row['seq'];
    }
    
    array_multisort($sequence, SORT_ASC, $children);
    
    $previous = null;
    $next = null;
    $flag = false;
    
    // find the previous page
    foreach($children as $child) {
      if ($pid == $child['pid']) {
        break;
      }
      else {
        $previous = $child;
      }
    }
    
    reset($children);
    
    // find the next page
    foreach($children as $child) {
      if ($flag) {
        $next = $child;
        break;
      }
      if ($pid == $child['pid']) {
        $flag = true;
      }
    }
    
    return array(
      'previous' => $previous, 
      'children' => $children, 
      'next' => $next
    );
  }

  public function ingest($files, $parent_id) {
    $connection = islandora_get_tuque_connection();
    $repository = $connection->repository;

    $namespace = explode(":", $parent_id);
    $namespace = $namespace[0];

    $page_object = $repository->constructObject($namespace);
    $page_object->relationships->add(FEDORA_MODEL_URI, 'hasModel', 'islandora:comparativeEditionPageCModel');
    $page_object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $parent_id);
    $page_object->relationships->add(ISLANDORA_RELS_EXT_URI, 'isSequenceNumber', $files['seq'], RELS_TYPE_PLAIN_LITERAL);

    // Construct MODS datastream
    $mods_datastream = $page_object->constructDatastream('MODS');
    $mods_datastream->label = 'MODS';
    $mods_datastream->mimeType = 'application/xml';
    $mods_datastream->setContentFromFile($files['mods']);
    $page_object->ingestDatastream($mods_datastream);
    $page_object->label = simplexml_load_file($files['mods'])->titleInfo->partNumber;

    // Construct OBJ datastream 
    $obj_datastream = $page_object->constructDatastream('OBJ');
    $obj_datastream->label = 'OBJ';
    $obj_datastream->mimeType = mime_content_type($files['obj']); 
    $obj_datastream->setContentFromFile($files['obj']);
    $page_object->ingestDatastream($obj_datastream);

    // Construct DISPLAYHTML datastream 
    $html_datastream = $page_object->constructDatastream('DISPLAYHTML');
    $html_datastream->label = 'DISPLAYHTML';
    $html_datastream->mimeType = 'text/html'; 
    $html_datastream->setContentFromFile($files['html']);
    $page_object->ingestDatastream($html_datastream);

    // Construct CONTENTXML datastream 
    $xml_datastream = $page_object->constructDatastream('CONTENTXML');
    $xml_datastream->label = 'CONTENTXML';
    $xml_datastream->mimeType = 'application/xml'; 
    $xml_datastream->setContentFromFile($files['xml']);
    $page_object->ingestDatastream($xml_datastream);
    
        // Construct HTMLPINDEX datastream 
    $html_pindex_datastream = $page_object->constructDatastream('HTMLPINDEX');
    $html_pindex_datastream->label = 'HTMLPINDEX';
    $html_pindex_datastream->mimeType = 'text/html'; 
    $html_pindex_datastream->setContentFromFile($files['html-pindex']);
    $page_object->ingestDatastream($html_pindex_datastream);


    // Construct TEXTPINDEX datastream 
    $text_pindex_datastream = $page_object->constructDatastream('TEXTPINDEX');
    $text_pindex_datastream->label = 'TEXTPINDEX';
    $text_pindex_datastream->mimeType = 'text/plain'; 
    $text_pindex_datastream->setContentFromFile($files['text-pindex']);
    $page_object->ingestDatastream($text_pindex_datastream);


    $page_id = $repository->ingestObject($page_object)->id;
  }

}

class IslandoraComparativeEditionWitnessIngestPackage { 

  public function validate($witness_url) {

    $validity = array(
      'valid' => TRUE, 
      'msg' => "The submitted witness package could not be ingested because it was missing the following parts:", 
      'files' => array(),
    ); 

    if (!is_dir($witness_url)) {
      $validity['valid'] = FALSE;
      $validity['msg'] = "<br/>- Error with unzipped witness at {$witness_url}."; 
      return $validity;
    }
    else {
      $witness_files = array();

      // Validate existence of witness MODS record
      $witness_files['mods'] = glob("{$witness_url}/*.xml");
      if (@file_exists($witness_files['mods'][0])) {
        $validity['files']['mods'] = $witness_files['mods'][0];
      }
      else {
        $validity['valid'] = FALSE;
        $validity['msg'] .= "<br/>- missing witness MODS record";
      }

      // Validate existence of witness image file 
      $witness_files['obj'] = glob("{$witness_url}/*.{jpg,jpeg,png,tiff}", GLOB_BRACE);
      if (@file_exists($witness_files['obj'][0])) {
        $validity['files']['obj'] = $witness_files['obj'][0];
      }
      else {
        $validity['valid'] = FALSE;
        $validity['msg'] .= "<br/>- missing witness image file";
      }

      // Validate existence of witness css file 
      $witness_files['css'] = glob("{$witness_url}/*.css");
      if (@file_exists($witness_files['css'][0])) {
        $validity['files']['css'] = $witness_files['css'][0];
      }
      else {
        $validity['valid'] = FALSE;
        $validity['msg'] .= "<br/>- missing witness css file";
      }

      $page_files = glob("{$witness_url}/pages/*");
      $pages = array();

      foreach ($page_files as $page_file) {
        $page_file_parts = explode(".", $page_file);
        $page_file_parts = explode("/", $page_file_parts[0]);
        $pages[] = end($page_file_parts);
      }

      // Sort alphabetically to determine page sequence number
      $pages = array_unique($pages);
      sort($pages);

      $i = 0;
      foreach ($pages as $page) {

        // Validate existence of page MODS record
        $witness_files['pages'][$page]['mods'] = "{$witness_url}/pages/{$page}.mods.xml";
        if (@file_exists($witness_files['pages'][$page]['mods'])) {
          $validity['files']['pages'][$page]['mods'] = $witness_files['pages'][$page]['mods'];
        }
        else {
          $validity['valid'] = FALSE;
          $validity['msg'] .= "<br/>- missing page MODS record from {$page}.";
        }

        // Validate existence of page XML file 
        $witness_files['pages'][$page]['xml'] = "{$witness_url}/pages/{$page}.content.xml";
        if (@file_exists($witness_files['pages'][$page]['xml'])) {
          $validity['files']['pages'][$page]['xml'] = $witness_files['pages'][$page]['xml'];
        }
        else {
          $validity['valid'] = FALSE;
          $validity['msg'] .= "<br/>- missing page content XML file from {$page}";
        }

        // Validate existence of page HTML file 
        $witness_files['pages'][$page]['html'] = "{$witness_url}/pages/{$page}.display.html";
        if (@file_exists($witness_files['pages'][$page]['html'])) {
          $validity['files']['pages'][$page]['html'] = $witness_files['pages'][$page]['html'];
        }
        else {
          $validity['valid'] = FALSE;
          $validity['msg'] .= "<br/>- missing page HTML file from {$page}";
        }
           
        // Validate existence of page HTMLPINDEX file 
        $witness_files['pages'][$page]['html-pindex'] = "{$witness_url}/pages/{$page}.index.html";
        if (@file_exists($witness_files['pages'][$page]['html-pindex'])) {
          $validity['files']['pages'][$page]['html-pindex'] = $witness_files['pages'][$page]['html-pindex'];
        }
        else {
          $validity['valid'] = FALSE;
          $validity['msg'] .= "<br/>- missing page HTMLPINDEX file from {$page}";
        }

        // Validate existence of page TEXTPINDEX file 
        $witness_files['pages'][$page]['text-pindex'] = "{$witness_url}/pages/{$page}.index.txt";
        if (@file_exists($witness_files['pages'][$page]['text-pindex'])) {
          $validity['files']['pages'][$page]['text-pindex'] = $witness_files['pages'][$page]['text-pindex'];
        }
        else {
          $validity['valid'] = FALSE;
          $validity['msg'] .= "<br/>- missing page TEXTPINDEX file from {$page}";
        }

        // Validate existence of page image file 
        $witness_files['pages'][$page]['obj'] = glob("{$witness_url}/pages/{$page}.obj.{jpg,jpeg,png,tiff}", GLOB_BRACE);
        if (@file_exists($witness_files['pages'][$page]['obj'][0])) {
          $validity['files']['pages'][$page]['obj'] = $witness_files['pages'][$page]['obj'][0];
        }
        else {
          $validity['valid'] = FALSE;
          $validity['msg'] .= "<br/>- missing page image file from {$page}";
        }

        // Increment counter and set as page sequence number
        $i++;
        $validity['files']['pages'][$page]['seq'] = $i;

      }

    }

    return $validity;
  }

  public function ingest($form_state) {
    $target = file_load($form_state['values']['target']);
    $parameters = $form_state['values'] + array(
      'zip_file_uri' => $target->uri,
      'type' => 'zip',
      'parent_relationship_uri' => 'info:fedora/fedora-system:def/relations-external#',
      'parent_relationship_pred' => 'isMemberOf',
      'wait_for_metadata' => FALSE,
      'directory_dedup' => FALSE,
      'content_models' => array('islandora:comparativeEditionWitnessCModel'),
    );

    $file_url = drupal_realpath($parameters['zip_file_uri']);
    $witness_url = variable_get('file_temporary_path', '/tmp') . "/cesp-witness";

    $zip = new ZipArchive;
    $response = $zip->open($file_url);
    if ($response === TRUE) {
      $zip->extractTo($witness_url);
      $zip->close();
    } else {
      watchdog('Islandora Solution Pack Comparative Edition', "Error extracting witness ingest zip package to tmp directory: '$witness_url'");
      exit();
    }

    $witness = $this->validate($witness_url);
    if ($witness['valid']) {
      $witness_object = new IslandoraComparativeEditionWitnessObject();
      $witness_object->ingest($parameters, $witness['files']);
    }
    else {
      drupal_set_message($witness['msg'], 'error');
    }

    array_map('unlink', glob("$witness_url/pages/{*.*,.DS_Store}", GLOB_BRACE));
    rmdir("$witness_url/pages");
    array_map('unlink', glob("$witness_url/{*.*,.DS_Store}", GLOB_BRACE));
    rmdir($witness_url);
    $deletion = file_delete($target);
  }

}
