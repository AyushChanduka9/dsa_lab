# Deployment Guide

This application is ready for deployment using Docker. This ensures it runs on any cloud provider (Render, Railway, AWS, DigitalOcean, etc.).

## 1. Prerequisites
- [Docker Desktop](https://www.docker.com/products/docker-desktop/) installed locally (for testing).
- A GitHub account (for cloud deployment).

## 2. Local Testing (Optional)
To verify everything works before deploying:

1. Open a terminal in the project root.
2. Build the Docker image:
   ```bash
   docker build -t banking-app .
   ```
3. Run the container:
   ```bash
   docker run -p 5000:5000 banking-app
   ```
4. Open `http://localhost:5000` in your browser.

## 3. Deploying to Cloud (Render.com - Free Tier)
1. **Push your code to GitHub**.
2. Go to [Render.com](https://render.com) and sign up.
3. Click **New +** -> **Web Service**.
4. Connect your GitHub repository.
5. Render will automatically detect the `Dockerfile`.
6. Click **Create Web Service**.

That's it! Render will build your app and give you a public URL (e.g., `https://banking-app.onrender.com`).

## Notes
- The frontend `API_URL` has been updated to relative `/api`, so it automatically works on the deployed URL.
- The backend has been updated to work on Linux (which Render uses).
