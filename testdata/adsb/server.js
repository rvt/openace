const readline = require("readline");
const net = require("net");
const fs = require("fs");

const args = process.argv.slice(2);
const [filePath, serverPort] = args;

// Create a server to listen for client connections
const server = net.createServer((socket) => {
  console.log("Client connected");

  // Function to send a line with a delay
  function sendLineWithDelay(line) {
    return new Promise((resolve, reject) => {
      socket.write(line + "\n", (err) => {
        if (err) {
          reject(err);
        } else {
          resolve();
        }
      });
    });
  }

  // Function to read lines from the file with a delay
  async function readLines() {
    const rl = readline.createInterface({
      input: fs.createReadStream(filePath),
      crlfDelay: Infinity,
    });

    let startVirtTs = null;
    let startRealTs = Date.now();
    let msgCount = 0;

    try {
      // Read each line with a delay
      for await (const line of rl) {
        const [timestamp, data] = line.split(" ", 2);
        const timestampInMs = parseFloat(timestamp) * 1000;

        if (startVirtTs === null) {
          startVirtTs = timestampInMs;
        }

        let delay = (timestampInMs - startVirtTs) - (Date.now() - startRealTs);

        if ((msgCount % 200) == 0) {
          console.log("msgCount: "+msgCount);
        }
        msgCount++;
        if (delay <= 0) {
          await sendLineWithDelay(data);
        } else {
          await sendLineWithDelay(data);
          await new Promise((resolve) => setTimeout(resolve, delay));
        }
      }
      // Handle the end of file reading
      console.log("File reading completed");

      // Loop back to the beginning of the file
      await readLines();
    } catch (error) {
      console.error("Error reading lines:", error);
    }
  }

  // Start reading lines when a client connects
  readLines().catch((error) => {
    console.error("Error in readLines:", error);
  });

  // Handle socket errors
  socket.on("error", (err) => {
    console.error("Socket error:", err);
  });

  // Handle socket close
  socket.on("close", () => {
    console.log("Client disconnected");
  });
});

server.on("listening", () => {
  console.log(`Server listening on port ${serverPort}`);
});

server.on("error", (err) => {
  console.error("Server error:", err);
});

server.listen(serverPort);
