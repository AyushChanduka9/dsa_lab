# Use a lightweight GCC image
FROM gcc:latest

# Set working directory
WORKDIR /app

# Copy backend source code
COPY backend/ .

# Compile the server (logic.c is included in server.c)
RUN gcc -o server server.c -I.

# Copy frontend files to a 'frontend' directory inside /app
COPY frontend/ ./frontend

# Expose the application port
EXPOSE 5000

# Run the server
CMD ["./server"]
