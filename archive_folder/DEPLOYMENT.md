# Deployment Guide: Safe Semantic Planner (SSP)

This guide provides step-by-step instructions to build, containerize, and deploy the Safe Semantic Planner web visualizer and REST API.

**Live Production Deployment**: [ssp.gabrieljames.me](https://ssp.gabrieljames.me)  

---

## 1. Live Production Service

The Safe Semantic Planner is continuously deployed and accessible globally over HTTPS:
- Production URL: [ssp.gabrieljames.me](https://ssp.gabrieljames.me)
- API Health Status: `https://ssp.gabrieljames.me/api/health`
- Interactive Visualizer: Available directly in any modern desktop or mobile browser.

---

## 2. Option 1: Multi-Stage Docker Web Service

The repository contains an optimized multi-stage `Dockerfile` and `render.yaml` blueprint.

### 2.1 Local Docker Build and Execution
```bash
# Build the production Docker container
docker build -t ssp:latest .

# Run the container mapping port 8080
docker run -p 8080:8080 ssp:latest
```
Access the application at `http://localhost:8080`.

### 2.2 Cloud Deployment (Render / AWS / GCP / DigitalOcean)
1. Push the repository to GitHub or GitLab.
2. Log into your cloud dashboard (e.g., [dashboard.render.com](https://dashboard.render.com)).
3. Create a New Web Service:
   - Connect your repository.
   - Environment: `Docker`
   - Instance Type: `Free` or standard tier.
4. Render automatically parses `render.yaml` and `Dockerfile`, compiling the C++17 binary in an isolated Alpine build stage and launching the runtime container.

---

## 3. Option 2: Native Build & Execution (No Docker)

For Linux and macOS host systems:

### 3.1 Build the Web Server Binary
```bash
# Compile server executable with maximum optimization
make bin/ssp_server
```

### 3.2 Launch the Server
```bash
# Start server (binds to 0.0.0.0:8080 or the PORT environment variable)
./bin/ssp_server
```

### 3.3 Custom Port Configuration
Set the `PORT` environment variable to override the default port (8080):
```bash
PORT=9000 ./bin/ssp_server
```

---

## 4. Verification and Health Checks

After launching the service, verify the server status using curl:

```bash
# Health check endpoint
curl -s http://localhost:8080/api/health

# Fetch current problem manifest
curl -s http://localhost:8080/api/problem | jq .

# Test natural language planning endpoint
curl -s -X POST http://localhost:8080/api/nlp_command \
  -H "Content-Type: application/json" \
  -d '{"query":"Find safe path from start to goal"}' | jq .
```
