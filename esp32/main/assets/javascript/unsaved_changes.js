var conf_complete = false;
$(function(){
    $(document).on('change', '#Network,#Engine,#Sensorheads', function() {
        if(conf_complete == true)
        {
            $("#you_have_unsaved_changes_panel").css("display", "block");
        }
    });
});