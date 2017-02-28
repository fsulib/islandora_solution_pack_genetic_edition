/* Javascript file for Comparative Editions Solution Pack Module */

jQuery(document).ready(function($) {
  
  $("#icesp-tabs").tabs();
  $("#icesp-tabs-comparison").tabs();
  
  $("#icesp-focus-button").click(function() {
    
    //disable focus button and enable comparison button
    $("#icesp-focus-button").prop('disabled', true);
    $("#icesp-comparison-button").removeClass('icesp-button-active');
    $("#icesp-focus-button").addClass('icesp-button-active');
    $("#icesp-comparison-button").removeAttr('disabled');
    
    //hide comparison view
    $("#icesp-tabs-comparison").css("display", "none");
    $("#icesp-tabs").removeClass('icesp-left-column');
    $("#icesp-tabs-comparison").removeClass('icesp-right-column');
    
  });
  
  $("#icesp-comparison-button").click(function() {
    
    //disable comparison button and enable focus button
    $("#icesp-comparison-button").prop('disabled', true);
    $("#icesp-focus-button").removeClass('icesp-button-active');
    $("#icesp-comparison-button").addClass('icesp-button-active');
    $("#icesp-focus-button").removeAttr('disabled');
    
    //show comparison view
    $("#icesp-tabs-comparison").css("display", "block");
    $("#icesp-tabs").addClass('icesp-left-column');
    $("#icesp-tabs-comparison").addClass('icesp-right-column');

  });
  
  var comp_units = document.getElementsByClassName('icesp-comparable');
 
  for(var i=0;i<comp_units.length;i++){
    comp_units[i].addEventListener("click", function(){
      var mindex_location = window.location.href + "/mindex/" + this.id;
      
      $.get(mindex_location, function( results_html ) {
        $("#icesp-dialog-text").text(results_html);
        $("#icesp-dialog").dialog();
      });      
      
    }, false);   
  }
  
});