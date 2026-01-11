var configuration = [];
var read_only = [];
var configuration_url;
var pt_touched = false;

const playtable_pattern = /playtable-(.*)\.txt/;
const version_regexp = /diceros_engine_(\d+)_(\d+)_(\d+).*/;
const oldest_backwards_compatible_version = new Array(3, 3, 0);

$.ajaxSetup({
    cache: false
});

if (!String.prototype.startsWith) {
    String.prototype.startsWith = function (searchString, position) {
        position = position || 0;
        return this.indexOf(searchString, position) === position;
    };
}

if (!String.prototype.endsWith) {
    String.prototype.endsWith = function (searchString, position) {
        if (!(position < this.length)) {
            position = this.length;
        }
        else {
            position |= 0; // round position
        }
        return this.substr(position - searchString.length, searchString.length) === searchString;
    };
}

function open_tab(evt, tabName) {
    var i, tabcontent, tablinks;
    tabcontent = document.getElementsByClassName("tabcontent");
    for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
    }
    tablinks = document.getElementsByClassName("tablinks");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }
    document.getElementById(tabName).style.display = "block";
    evt.currentTarget.className += " active";
}

function is_version_compatible(firmware_filename) {
    matches = version_regexp.exec(firmware_filename);

    if (matches !== null) {
        for (i = 0; i < 3; i++) {
            if (matches[i + 1] > oldest_backwards_compatible_version[i]) {
                return true;
            }
            else if (matches[i + 1] < oldest_backwards_compatible_version[i]) {
                return false;
            }
        }

        // Version is identical, should be compatible.
        return true;
    }
    else {
        return false;
    }
}

function pad(str, max) {
    str = str.toString();
    return str.length < max ? pad("0" + str, max) : str;
}


function update_front_for_hidden_ui(field, value) {
    // set UI checkbox for hiddem item
    let checkbox = $("#" + field + "_checkbox");
    if (checkbox) {
        checkbox.prop("checked", (value == "enabled"));
    }

    // set UI selector for hidden item
    let selector = $("#" + field + "_selector");
    if (selector) {
        let selector_option = $(selector).find("option").filter(function (i, e) { return $(e).val().toLowerCase() == String(value).toLowerCase() });

        if (selector_option.length == 0) {
            selector.append($("<option>").val(value).html(option_formatter(field, value)));
        }
        selector.val(value);
    }
}

function update_status_start() {
    update_status_handler();
}

function exec_if_not_idling(callback) {
    rest_call('/status/get', function (data) {
        console.log(data);
        var running = data.running;

        if (running) {
            alert('TV-Slider must be idle.');
        }
        else {
            if (callback) {
                callback();
            }
        }
    });
}

function download_configuration(form) {
    exec_if_not_idling(function () {
        form.submit(); // only if the engine is idle trigger the form submit
    });

    return false; // prevent the default action
}

function update_status_handler() {
    rest_call('/status/get', function (data) {
        console.log(data);
        var device_status = data.Mode;
        $('#device_mode').text(device_status);

        if (device_status == 'Error') {
            if (typeof show_error_reason === "function") {
                $('#device_mode').html("Error <a href='#' onclick='show_error_reason(" + data.Reason + ")'>" + data.Reason + "</a>")
            }
            else {
                $('#device_mode').html("Error " + data.Reason)
            }
        }
    }, function () {
        // start next request after this one is finished
        setTimeout(function () {
            update_status_handler()
        }, 1000);
    });
}

function get_configuration() {
    rest_call(configuration_url, function (data) {
        configuration = [];
        console.log(data);

        for (var configuration_item in data) {
            var element = $('#' + configuration_item);

            if (element.is("input")) {
                configuration[configuration_item] = data[configuration_item];

                read_only[configuration_item] = element.prop('disabled');

                element.val(data[configuration_item]);

                if (element.attr("type") == "hidden") {
                    update_front_for_hidden_ui(configuration_item, data[configuration_item]);
                }
            }
            else if (element.is("select")) {
                configuration[configuration_item] = data[configuration_item];

                read_only[configuration_item] = element.prop('disabled');

                var option_obj = $(element).find('option').filter(function () {
                    return this.value.toLowerCase() == data[configuration_item].toString().toLowerCase();
                });

                option_obj.attr("selected", "selected");

                $(element).val(option_obj.val());
            }
            else if (element.is("div")) {
                // this is a read-only attribute
                if (element.attr("length") !== undefined) {
                    // padding to attribute length
                    $(element).text(pad(data[configuration_item], parseInt(element.attr("length"))));
                }
                else {
                    $(element).text(data[configuration_item]);
                }
            }

            $(element).trigger('change');
        }
        // from hereon, if any input data is modified, user needs to be reminded of unsaved changes
        conf_complete = true;
    });
}

function save_configuration() {
    var save_failed = false;

    console.log(configuration)
    var query_items = new Array();

    for (var configuration_item in configuration) {
        if (!/^mfn.*/.exec(configuration_item) && !read_only[configuration_item]) {
            configuration[configuration_item] = $('#' + configuration_item).val();
            query_items.push(configuration_item + '=' + configuration[configuration_item]);
        }
    }

    $.post('/Configuration/Set?' + query_items.join('&'), function (data) {
        console.log(data);
        if (data.Error != 0) {
            save_failed = true;

            if (typeof show_error_reason === "function") {
                show_error_reason(data.Error, 'Configuration not saved, faulty property value: ' + data.FaultyProperty);
            }
            else {
                alert('Not all configuration properties may have been saved.');
            }
        }
        if (!save_failed) {
            alert('Configuration saved');
        }
    });

    // Set current playtable
    if (pt_touched) {
        rest_call('/playtable/set?playtable=' + $("#pt_selection").val(), function (data) { });
        pt_touched = false;
    }

    document.getElementById("you_have_unsaved_changes_panel").style.display = "none";
}

function set_reboot() {
    rest_call('/reboot', function (data) {
        console.log(data);
        if (data.Error != 0) {
            alert('Unable to reboot engine');
        }

        setTimeout(function () {
            console.log('Restarting client')
            window.location.href = '/';
        }, 5000);
    });
}

function rest_call(uri, callback, complete_callback) {
    $.ajax({
        url: uri,
        success: callback,
        complete: complete_callback,
        error: function (xhr, error_status, error_thrown) {
            console.log(error_status);
        }
    });
}

function get_files(pattern, callback) {
    var uri = "/files/list";

    $.ajax({
        url: uri,
        cache: false,
        success: function (data) {
            files = [];

            console.log(data.files)

            for (var index in data.files) {
                var file = data.files[index];

                if (pattern.exec(file)) {
                    files.push(file);
                }
            }

            files.sort();

            callback(files);
        }
    });
}

function load_to_select(id, pattern, callback) {
    get_files(pattern, function (playtables) {

        $("#" + id).find("option").remove();

        for (var index in files) {
            if (id.constructor == Array) {
                for (var i in id) {
                    $("#" + i).append($("<option>")
                        .val(files[index])
                        .html(option_formatter(id, files[index]))
                    );
                }
            }
            else {
                $("#" + id).append($("<option>")
                    .val(files[index])
                    .html(option_formatter(id, files[index]))
                );

            }
        }

        if (callback) {
            callback();
        }
    });
}

function sensorhead_upload(progress_callback, success_callback) {
    var url = "/Motor/Writeimage";
    var data = new FormData();
    var ok = false;

    console.log($("#sensorhead_upgrade_input")[0].files);

    $.each($("#sensorhead_upgrade_input")[0].files, function (i, file) {
        if (file.name.startsWith('gohan')) {
            data.append('sensorhead', document.getElementById("sensorhead_id_option").value);
            data.append(file.name, file);
            ok = true;
        }
        else {
            alert('Filename not allowed.');
        }
    });

    if (ok) {
        $("#sensorhead_upgrade_panel_form").hide();

        $.ajax({
            url: url,
            data: data,
            cache: false,
            contentType: false,
            processData: false,
            type: "POST",
            xhr: function () {
                var xhr = $.ajaxSettings.xhr();
                if (xhr.upload) {
                    xhr.upload.addEventListener('progress', progress_callback, false);
                }
                return xhr;
            },
            success: function (data) {
                console.log(data);
                success_callback(data);
            }
        });
    }
}

function upload_firmware(progress_callback, success_callback) {
    var url = "/update";
    var file = $("#firmware_upgrade_input")[0].files[0];

    if (!file) {
        alert("Select a .bin file first.");
        return;
    }

    if (!file.name.endsWith(".bin")) {
        alert("Filename not allowed.");
        return;
    }

    exec_if_not_idling(function () {
        $("#firmware_upgrade_panel_form").hide();

        $.ajax({
            url: url,
            data: file,
            cache: false,
            contentType: "application/octet-stream",
            processData: false,
            type: "POST",
            xhr: function () {
                var xhr = $.ajaxSettings.xhr();
                if (xhr.upload) {
                    xhr.upload.addEventListener('progress', progress_callback, false);
                }
                return xhr;
            },
            success: function (data) {
                console.log(data);
                success_callback(data);
            },
            error: function () {
                $("#firmware_upgrade_panel_progress").html("<h2>Failed</h2>");
                $("#firmware_upgrade_panel_form").show();
            }
        });
    });
}

function sensorhead_upload_progress(event_data) {
    if (event_data.lengthComputable) {
        var max = event_data.total;
        var current = event_data.loaded;

        var percentage = Math.round((current * 100) / max);
        console.log(percentage);

        if (percentage >= 99) {
            percentage = 99;
        }

        $("#sensorhead_upgrade_panel_progress").html("<h2>" + percentage + "%</h2>");
    }
}

function upload_firmware_progress(event_data) {
    if (event_data.lengthComputable) {
        var max = event_data.total;
        var current = event_data.loaded;

        var percentage = Math.round((current * 100) / max);
        console.log(percentage);

        if (percentage >= 99) {
            percentage = 99;
        }

        $("#firmware_upgrade_panel_progress").html("<h2>" + percentage + "%</h2>");
    }
}

function sensorhead_upload_success(data) {
    if (data.Error != 0) {
        if (typeof show_error_reason === "function") {
            $("#sensorhead_upgrade_panel_progress").html("<h2>Failed, error: <a href='#' onclick='show_error_reason(" + data.Error + ")'>" + data.Error + "</a></h2>");
        }
        else {
            $("#sensorhead_upgrade_panel_progress").html("<h2>Failed, error: " + data.Error + "</h2>");
        }
        $("#sensorhead_upgrade_panel_form").show();
    }
    else {
        $("#sensorhead_upgrade_panel_progress").html("Success, revision: " + ((data.Revision >> 24) & 255) + '.' + ((data.Revision >> 16) & 255) + '.' + ((data.Revision >> 8) & 255) + '.' + (data.Revision & 255));
        $("#sensorhead_upgrade_panel_form").show();
    }
}

function upgrade_firmware_success(data) {
    if (typeof data === "object" && data !== null && data.Error !== undefined) {
        if (data.Error != 0) {
            if (typeof show_error_reason === "function") {
                $("#firmware_upgrade_panel_progress").html("<h2>Failed, error: <a href='#' onclick='show_error_reason(" + data.Error + ")'>" + data.Error + "</a></h2>");
            }
            else {
                $("#firmware_upgrade_panel_progress").html("<h2>Failed, error: " + data.Error + "</h2>");
            }
            $("#firmware_upgrade_panel_form").show();
            return;
        }
    }

    set_reboot();
    $("#firmware_upgrade_panel_progress").html("<h2>Success</h2>Reboot required<br/>The engine is being restarted...");
}


function restore_configuration(progress_callback, success_callback) {
    var url = "/configuration/upload";
    var data = new FormData();
    var ok = false;

    console.log($("#restore_configuration_input")[0].files);

    exec_if_not_idling(function () {

        $.each($("#restore_configuration_input")[0].files, function (i, file) {
            if (file.name.endsWith('.cfg') && file.name.startsWith('diceros_')) {
                data.append("configuration", file);

                ok = confirm('Uploading an invalid configuration file will erase your current configuration. Are you sure you want to continue?');
            }
            else {
                alert('Filename not allowed.');
            }
        });

        if (ok) {
            $("#restore_configuration_panel_form").hide();

            $.ajax({
                url: url,
                data: data,
                cache: false,
                contentType: false,
                processData: false,
                type: "POST",
                xhr: function () {
                    var xhr = $.ajaxSettings.xhr();
                    if (xhr.upload) {
                        xhr.upload.addEventListener('progress', progress_callback, false);
                    }
                    return xhr;
                },
                success: function (data) {
                    console.log(data);
                    success_callback(data);
                }
            });
        }
    });
}

function restore_configuration_progress(event_data) {
    if (event_data.lengthComputable) {
        var max = event_data.total;
        var current = event_data.loaded;

        var percentage = Math.round((current * 100) / max);
        console.log(percentage);

        if (percentage >= 99) {
            percentage = 99;
        }

        $("#restore_configuration_panel_progress").html("<h2>" + percentage + "%</h2>");
    }
}

function restore_configuration_success(data) {
    if (data.Error != 0) {
        if (typeof show_error_reason === "function") {
            $("#restore_configuration_panel_progress").html("<h2>Failed, error: <a href='#' onclick='show_error_reason(" + data.Error + ")'>" + data.Error + "</a></h2>");
        }
        else {
            $("#restore_configuration_panel_progress").html("<h2>Failed, error: " + data.Error + "</h2>");
        }
        $("#restore_configuration_panel_form").show();
    }
    else {
        set_reboot();
        $("#restore_configuration_panel_progress").html("<h2>Success</h2>Reboot required<br/>The engine is being restarted...");
    }
}


