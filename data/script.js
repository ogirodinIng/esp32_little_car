var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
    const videoContainer = document.querySelector('.video-stream');

    // Créer et ajouter le titre h2
    const title = document.createElement('h2');
    title.textContent = 'Video Stream';
    videoContainer.appendChild(title);
    
    // Créer et ajouter l'image
    const videoImage = document.createElement('img');
    videoImage.id = 'videoStream';
    videoImage.src = `http://${window.location.hostname}:81/stream`;
    videoImage.style.width = '100%';
    videoImage.style.maxWidth = '600px';
    videoImage.alt = 'Video Stream';
    videoContainer.appendChild(videoImage);
    videoContainer.appendChild(title);

    initWebSocket();

    document.getElementById('toggle-blue-light').addEventListener('mousedown', function() {
        websocket.send("toggleBlueLight");
    });
    document.getElementById('toggle-blue-light').addEventListener('mouseup', function() {
        websocket.send("toggleBlueLight");
    });

    // gestion du joystick 
    let lastSentTime = 0; // Timestamp of the last sent data
    const sendInterval = 50; // Minimum interval in milliseconds

    const joystick = document.getElementById('joystick');
    const joystickContainer = document.getElementById('joystick-container');
    const directionDisplay = document.getElementById('direction');
    const valueDisplay = document.getElementById('value');

    const maxRadius = joystickContainer.offsetWidth / 2 - joystick.offsetWidth / 2;
    const centerX = joystickContainer.offsetWidth / 2;
    const centerY = joystickContainer.offsetHeight / 2;

    let isDragging = false;
    let lastDirection = null;

    joystick.addEventListener('mousedown', startDrag);
    joystick.addEventListener('touchstart', startDrag);

    document.addEventListener('mouseup', stopDrag);
    document.addEventListener('touchend', stopDrag);

    document.addEventListener('mousemove', drag);
    document.addEventListener('touchmove', drag);

    function startDrag(e) {
        e.preventDefault();
        isDragging = true;
    }

    function stopDrag() {
        if (isDragging) {
            // Reset joystick to center when released
            joystick.style.transform = `translate(0px, 0px)`;
            updateDirection(0, 0); // Reset direction to center
        }
        isDragging = false;
    }

    function drag(e) {
        if (!isDragging) return;

        let clientX, clientY;

        if (e.touches) {
            clientX = e.touches[0].clientX;
            clientY = e.touches[0].clientY;
        } else {
            clientX = e.clientX;
            clientY = e.clientY;
        }

        const rect = joystickContainer.getBoundingClientRect();
        const x = clientX - rect.left - centerX;
        const y = clientY - rect.top - centerY;

        const distance = Math.sqrt(x * x + y * y);
        const clampedDistance = Math.min(distance, maxRadius);

        const newX = clampedDistance * (x / distance);
        const newY = clampedDistance * (y / distance);

        joystick.style.transform = `translate(${newX}px, ${newY}px)`;

        updateDirection(newX, newY);
    }

    function updateDirection(x, y) {
        let direction = '';
        let value = 0;

        if (Math.abs(y) >= Math.abs(x)) {
            if (y < 0) {
                direction = 'N';
                value = mapValue(-y, 0, maxRadius, 150, 255);
            } else if (y > 0) {
                direction = 'S';
                value = mapValue(y, 0, maxRadius, 150, 255);
            }
        } else {
            if (x > 0) {
                direction = 'E';
                value = mapValue(x, 0, maxRadius, 150, 255);
            } else if (x < 0) {
                direction = 'O';
                value = mapValue(-x, 0, maxRadius, 150, 255);
            }
        }

        if (direction !== lastDirection || value !== valueDisplay.textContent) {
            lastDirection = direction;
            directionDisplay.textContent = direction;
            valueDisplay.textContent = value;

            const currentTime = Date.now();
            if (currentTime - lastSentTime >= sendInterval || !direction) {
                // Simulate sending the new direction and value
                console.log(`Sending: ${direction} (${value})`);
                websocket.send(JSON.stringify({joystick: value, direction}));
                lastSentTime = currentTime;
            }
        }
    }

    function mapValue(value, inMin, inMax, outMin, outMax) {
        return Math.round((value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin);
    }
}

function getReadings(){
    websocket.send("getReadings");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
    console.log('Connection opened');
    getReadings();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

// Function that receives the message from the ESP32 with the readings
var interval;
function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);
    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        if (!!myObj[key] && key==='alarmSound') {
            document.getElementById(key).classList.replace('off', 'on');
            const alarmSound = document.getElementById('alarmSound');
            interval = setInterval(function() {
                alarmSound.play();
            }, 1000);
        } else if(key==='pir') {
            document.getElementById(key).classList.replace('on', 'off');
            clearInterval(interval);
        }
    }
}