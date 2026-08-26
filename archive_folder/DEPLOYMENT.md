# Deployment Guide: Hosting Safe Semantic Planner (SSP) on Render

This guide provides step-by-step instructions to deploy the Safe Semantic Planner web visualizer and REST API to [Render.com](https://render.com) for free.

---

## Option 1: Docker Web Service (Recommended — 1-Click)

The repository already includes an optimized multi-stage `Dockerfile` and `render.yaml` blueprint.

### Step-by-Step:
1. **Push your code to GitHub / GitLab**:
   ```bash
   git add .
   git commit -m "Configure Render Docker deployment"
   git push origin main
   ```

2. **Log into Render**:
   - Go to [dashboard.render.com](https://dashboard.render.com).

3. **Create New Web Service**:
   - Click **New +** $\to$ **Web Service**.
   - Connect your GitHub / GitLab repository (`SSP`).

4. **Configure Service**:
   - **Name**: `safe-semantic-planner` (or your choice)
   - **Region**: Any (e.g. *Oregon, USA* or *Frankfurt, EU*)
   - **Branch**: `main`
   - **Environment**: **`Docker`**
   - **Instance Type**: **`Free`**

5. **Deploy**:
   - Click **Create Web Service**.
   - Render will build the multi-stage Docker container and launch the web server automatically.

---

## Option 2: Native Native Build (No Docker)

If you prefer building directly on Render's native Linux environment:

1. In Render, select **Environment**: **`Native / Custom`** (Linux).
2. Set the configuration fields:
   - **Build Command**:
     ```bash
     make bin/ssp_server
     ```
   - **Start Command**:
     ```bash
     ./bin/ssp_server
     ```
3. In **Environment Variables**, Render will automatically inject `$PORT`. The server is pre-configured to bind to `0.0.0.0:$PORT`.

---

## Verifying Your Live Deployment

Once the deploy status turns green (**Live**):
1. Click your Render URL: `https://safe-semantic-planner-xxxx.onrender.com`
2. You will see the interactive **2D / 3D Visualizer Dashboard**.
3. All REST endpoints (`/api/problem`, `/api/plan`, `/api/nlp_command`, `/api/export_path`) are immediately accessible over HTTPS!

---

## Live Production Deployment

The SSP web visualizer is deployed and accessible at:
- **URL**: [ssp.gabrieljames.me](https://ssp.gabrieljames.me)

