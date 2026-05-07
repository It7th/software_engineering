FROM node:22-alpine

WORKDIR /app

COPY package*.json ./
RUN npm install --omit=dev

COPY src ./src

ENV PORT=8080
ENV MONGO_URL=mongodb://mongo:27017/lms_mongo

EXPOSE 8080

CMD ["npm", "start"]
