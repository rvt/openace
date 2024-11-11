const { createProxyMiddleware } = require("http-proxy-middleware");

//const TARGET = "192.168.1.1";
const TARGET = "192.168.178.227";

function capitalizeHeader(header) {
  return header.replace(/\b\w/g, (char) => char.toUpperCase());
}

module.exports = function (app) {
  app.use(
    "/api",
    createProxyMiddleware({
      target: `http://${TARGET}:80/api`,
      changeOrigin: false,
      secure: false,

      on: {
        proxyReq: (proxyReq, req, res) => {
          const keepHeaders = ["host", "content-length", "accept", "x-method", "content-type", "connection"];
          const proxyHeaders = Object.keys(proxyReq.getHeaders());

          proxyHeaders.forEach((header) => {
            if (!keepHeaders.some((keepHeader) => header.startsWith(keepHeader))) {
              proxyReq.removeHeader(header);
            } else {
              // Headers in LWiP are case sensitive, so here we set them correct.
              // Altough spec says it should be case insenitive createProxyMiddleware still does change them
              proxyReq.setHeader(capitalizeHeader(header), proxyReq.getHeader(header));
            }
          });
        },

        error: (err, req, res) => {
          console.error("Proxy error:", err);
          res.writeHead(500, {
            "Content-Type": "text/plain",
          });
          res.end("Something went wrong with the proxy server.");
        },
      },
    }),
  );
};
