// Establishing WebSocket connection
const SERVER_ENDPOINT = 'ws://baobao.local/wsled';
const socket = new WebSocket(SERVER_ENDPOINT);

// Sends a message to the http server
// it flips the switch based on the current lighting status
const DEFAULT_LED_STATUS = "default_led_status";
let current_led_status = "";
function SendLedControlMessage() {
    let desired_state = null;
    if (current_led_status == "on") {
        desired_state = "off";
    }
    else if (current_led_status == "off") {
        desired_state = "on";
    }
    else {
        console.log(`Invalid current led status ${current_led_status}`);
        return;
    }

    const message = {
        state: desired_state
    };

    if (socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify(message));
        console.log("Sent: " + JSON.stringify(message));
    } else {
        console.log("Connection is not open.");
    }
}

// Function to display messages on the HTML page
function displayMessage() {
    const ledStatus = document.getElementById('ledStatus');
    ledStatus.textContent = current_led_status;
}


// WebSocket event listeners
socket.addEventListener('open', function (event) {
    console.log("Connection opened.");
});

socket.addEventListener('message', function (event) {
    console.log('Message from ESP32: ', event.data);
    try {
        const json = JSON.parse(event.data);
        console.log(json);
        current_led_status = json.status;
        displayMessage();
    }
    catch (error) {
        console.log(error);
    }
});

socket.addEventListener('error', function (event) {
    console.error('WebSocket error: ', event);
});

socket.addEventListener('close', function (event) {
    console.log('WebSocket connection closed.');
});


// DOMContentLoaded event to attach event listeners
document.addEventListener('DOMContentLoaded', function () {
    const sendButton = document.getElementById('ledControlButton');
    sendButton.addEventListener('click', SendLedControlMessage);
});


// Ensure to close WebSocket connection on page unload
window.addEventListener('beforeunload', function () {
    if (socket.readyState === WebSocket.OPEN) {
        socket.close();
        console.log("WebSocket connection closed");
    }
});