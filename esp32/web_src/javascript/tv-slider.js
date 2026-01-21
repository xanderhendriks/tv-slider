$.ajaxSetup({
    cache: false
});

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

var tv_position_timer = null;

function init_tv_view() {
    var $view = $("#tv-view");
    var $unknown = $("#tv-unknown");
    var $hidden_layer = $("#tv-hidden-layer");
    var $sliding_layer = $("#tv-sliding-layer");

    function apply_size_from_image(img) {
        if (!img || !img.naturalWidth || !img.naturalHeight) {
            return;
        }
        $view.css({
            width: img.naturalWidth + "px",
            height: img.naturalHeight + "px"
        });
    }

    $("#tv-hidden, #tv-sliding, #tv-unknown").on("load", function () {
        apply_size_from_image(this);
    });

    apply_size_from_image($("#tv-hidden")[0] || $("#tv-sliding")[0] || $("#tv-unknown")[0]);
    $unknown.show();
    $hidden_layer.hide();
    $sliding_layer.hide();
}

function render_tv_position(position) {
    var $unknown = $("#tv-unknown");
    var $hidden_layer = $("#tv-hidden-layer");
    var $sliding_layer = $("#tv-sliding-layer");

    if (position === undefined || position === null || position <= -1) {
        $unknown.show();
        $hidden_layer.hide();
        $sliding_layer.hide();
        return;
    }

    var clamped = Math.max(0, Math.min(100, position));
    $unknown.hide();
    $hidden_layer.show();
    $sliding_layer.show();
    $hidden_layer.css("width", (100 - clamped) + "%");
    $sliding_layer.css("width", clamped + "%");
}

function update_position_handler() {
    rest_call('/position/get', function (data) {
        var position = parseInt(data && data.position, 10);
        if (isNaN(position)) {
            position = -1;
        }
        render_tv_position(position);
    }, function () {
        tv_position_timer = setTimeout(function () {
            update_position_handler();
        }, 150);
    });
}

function start_position_updates() {
    if (tv_position_timer) {
        return;
    }
    update_position_handler();
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

function load_info() {
    rest_call('/info/get', function (data) {
        $("#info_hostname").text(data.hostname || "");
        $("#info_ip").text(data.ip_address || "");
        $("#info_mac").text(data.mac_address || "");
        $("#info_app_version").text(data.app_version || "");
        $("#info_compile_time").text(data.compile_time || "");
        $("#info_idf_version").text(data.idf_version || "");
        $("#info_mcu").text(data.mcu || "");
        $("#info_flash").text(data.flash || "");
        $("#info_heap").text(data.min_heap || "");
    });
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

    $("#firmware_upgrade_panel_progress").html("<h2>Success</h2>Reboot required<br/>The engine is being restarted...");
    setTimeout(function () {
        console.log('Restarting client')
        window.location.href = '/';
    }, 2000);
}

function load_config_handler() {
    rest_call('/config/get', function (data) {
        if (data) {
            $("#config_mqtt_server").val(data.mqtt_server || "");
            $("#config_mqtt_port").val(data.mqtt_port != null ? data.mqtt_port : "");
            $("#config_mqtt_topic").val(data.mqtt_topic || "");
            $("#config_invert_inputs").prop("checked", data.invert_inputs || false);
            $("#config_status_message").text("Configuration loaded").css("color", "green");
            setTimeout(function () {
                $("#config_status_message").text("");
            }, 3000);
        }
    });
}

function save_config_handler() {
    var config_data = {
        mqtt_server: $("#config_mqtt_server").val(),
        mqtt_port: parseInt($("#config_mqtt_port").val(), 10) || 1883,
        mqtt_topic: $("#config_mqtt_topic").val(),
        invert_inputs: $("#config_invert_inputs").is(":checked")
    };

    $.ajax({
        url: '/config/set',
        type: 'POST',
        contentType: 'application/json',
        data: JSON.stringify(config_data),
        success: function (response) {
            $("#config_status_message").text("Configuration saved successfully").css("color", "green");
            setTimeout(function () {
                $("#config_status_message").text("");
            }, 3000);
        },
        error: function (xhr, status, error) {
            $("#config_status_message").text("Failed to save configuration").css("color", "red");
        }
    });
}

function slider_post_event(event_name) {
    $.ajax({
        url: '/slider/event',
        type: 'POST',
        contentType: 'application/json',
        data: JSON.stringify({ event: event_name }),
        success: function (response) {
            console.log("Event posted: " + event_name);
        },
        error: function (xhr, status, error) {
            console.error("Failed to post event: " + event_name);
        }
    });
}

function slider_move_in() {
    slider_post_event("cmd_move_in");
}

function slider_move_out() {
    slider_post_event("cmd_move_out");
}

function slider_stop() {
    slider_post_event("cmd_stop");
}

if (typeof window !== "undefined") {
    window.open_tab = open_tab;
    window.rest_call = rest_call;
    window.init_tv_view = init_tv_view;
    window.start_position_updates = start_position_updates;
    window.update_status_start = update_status_start;
    window.exec_if_not_idling = exec_if_not_idling;
    window.update_status_handler = update_status_handler;
    window.load_info = load_info;
    window.upload_firmware = upload_firmware;
    window.upload_firmware_progress = upload_firmware_progress;
    window.upgrade_firmware_success = upgrade_firmware_success;
    window.load_config_handler = load_config_handler;
    window.save_config_handler = save_config_handler;
    window.slider_post_event = slider_post_event;
    window.slider_move_in = slider_move_in;
    window.slider_move_out = slider_move_out;
    window.slider_stop = slider_stop;
}
