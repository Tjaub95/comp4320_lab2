package com.comp4320;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class UDPClient {
    private static final int MAX_BUFF_LEN = 1000;
    private static final int GID = 12;
    private static final int MAGIC_NUMBER = 0x4A6F7921;

    public static void main(String[] args) throws IOException {

        // Check command line args len
        if (args.length < 4) {
            System.err.println("Parameters required: serverName portNum requestID, and at least one hostName");
            System.exit(1);
        }

        // Check if within valid range
        if (Integer.parseInt(args[2]) < 0 || Integer.parseInt(args[2]) > 127) {
            System.err.println("The requestID must be between 0 and 127");
            System.exit(1);
        }

        String serverName = args[0];
        int portNum = Integer.parseInt(args[1]);
        // requestID is 1 byte
        byte requestID = args[2].getBytes()[0];

        String[] hostnames = new String[args.length - 3];
        // Copy hostnames in args array to hostnames array
        System.arraycopy(args, 3, hostnames, 0, hostnames.length);

        // Create a new dgram
        DatagramSocket packetSocket = new DatagramSocket();
        // Get the IP of the server being connected to
        InetAddress serverAddress = InetAddress.getByName(serverName);

        byte[] sendMessage = new byte[MAX_BUFF_LEN - 9];
        byte[] receivedMessage = new byte[MAX_BUFF_LEN - 9];
        // Magic Number in Network Order
        System.arraycopy(intToByte(MAGIC_NUMBER), 0, sendMessage, 0, 4);

        sendMessage[6] = (byte) GID;
        sendMessage[7] = (byte) 0;
        sendMessage[8] = requestID;

        int index = 9;
        for (String hostname : hostnames) {
            sendMessage[index] = (byte) hostname.getBytes("UTF-8").length;
            index++;

            System.arraycopy(hostname.getBytes("UTF-8"), 0, sendMessage, index, hostname.getBytes("UTF-8").length);

            index += hostname.getBytes("UTF-8").length;
        }

        short length = (short) index;
        System.arraycopy(shortToByte(length), 0, sendMessage, 4, 2);

        sendMessage[7] = calculateChecksum(sendMessage);

        int totalAttempted = 1;
        while (totalAttempted <= 7) {
            DatagramPacket sendPacket = new DatagramPacket(sendMessage, length, serverAddress, portNum);
            System.out.println("Sending packet...");
            packetSocket.send(sendPacket);
            System.out.println("Packet sent...");

            DatagramPacket receivePacket = new DatagramPacket(receivedMessage, receivedMessage.length);
            System.out.println("Waiting for response from server...");
            packetSocket.receive(receivePacket);
            System.out.println("Packet received...");

            byte[] response = receivePacket.getData();

            if (response[8] == (byte) 32) {
                totalAttempted++;
                if (totalAttempted == 8) {
                    System.out.println("After 7 attempts, the server returned an invalid message");
                }

                System.out.println("A magic number was missing in the header, it was corrupted, or the wrong one was provided");
            } else if (response[8] == (byte) 64) {
                totalAttempted++;
                if (totalAttempted == 8) {
                    System.out.println("After 7 attempts, the server returned an invalid message");
                }

                System.out.println("When the checksum was calculated on the server it didn't equal -1, meaning the message was corrupted or the CRC was calculated incorrectly");
            } else if (response[8] == (byte) 128) {
                totalAttempted++;
                if (totalAttempted == 8) {
                    System.out.println("After 7 attempts, the server returned an invalid message");
                }

                System.out.println("The message sent was too short/long.");
            } else {
                int received_magic_number = bytesToInt(response, 0);
                received_magic_number = Integer.reverseBytes(received_magic_number);
                int received_tml = bytesToShort(response[4], response[5]);
                byte received_gid = response[6];
                byte received_checksum = response[7];
                byte received_rid = response[8];

                byte[] received_ips = new byte[received_tml - 9];
                String[] full_ip_addr = new String[received_ips.length / 4];
                System.arraycopy(response, 9, received_ips, 0, received_tml - 9);

                if (received_magic_number == MAGIC_NUMBER) {

                    byte sum = (byte) 0;
                    for (byte aResponse : response) {
                        sum += aResponse;
                    }

                    if (sum == (byte) -1) {
                        for (int i = 0; i < received_ips.length / 4; i++) {
                            String[] parts = new String[4];
                            int value = received_ips[4 * i];
                            if (value < 0)
                                value += 256;
                            parts[0] = Integer.toString(value);
                            value = received_ips[4 * i + 1];
                            if (value < 0)
                                value += 256;
                            parts[1] = Integer.toString(value);
                            value = received_ips[4 * i + 2];
                            if (value < 0)
                                value += 256;
                            parts[2] = Integer.toString(value);
                            value = received_ips[4 * i + 3];
                            if (value < 0)
                                value += 256;
                            parts[3] = Integer.toString(value);

                            String concat = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
                            full_ip_addr[i] = concat;
                        }

                        System.out.println("Server Response:");

                        System.out.println("Magic Number: " + Integer.toHexString(received_magic_number));
                        System.out.println("Total message length: " + received_tml);
                        System.out.println("Group ID: " + received_gid);
                        System.out.println("Checksum: " + received_checksum);
                        System.out.println("Request ID: " + received_rid);

                        for (int i = 0; i < full_ip_addr.length; i++) {
                            System.out.print(hostnames[i]);
                            System.out.print(": ");
                            System.out.println(full_ip_addr[i]);
                        }

                        totalAttempted = 8;
                    } else {
                        totalAttempted++;
                        System.out.println("The server's response was corrupted; trying again");
                    }
                }


                totalAttempted++;
            }
        }

        packetSocket.close();

    }

    private static byte[] intToByte(int intToConv) {
        return ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN).putInt(intToConv).array();
    }

    private static byte[] shortToByte(short shortToConv) {
        return ByteBuffer.allocate(2).order(ByteOrder.BIG_ENDIAN).putShort(shortToConv).array();
    }

    private static short bytesToShort(byte firstHalfOfShort, byte secondHalfOfShort) {
        return (short) ((firstHalfOfShort << 8) | secondHalfOfShort);
    }

    private static int bytesToInt(byte[] bytes, int offset) {
        int ret = 0;
        for (int i = 0; i < 4 && i+offset < bytes.length; i++) {
            ret <<= 8;
            ret |= (int) bytes[i] & 0xFF;
        }

        return ret;
    }

    private static byte calculateChecksum(byte[] message) {
        byte sum = (byte) 0;

        for (byte aMessage : message) {
            sum += aMessage;
        }

        return (byte) ~sum;
    }
}
