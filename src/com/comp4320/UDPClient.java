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
        System.arraycopy(intToByte(1248819489), 0, sendMessage, 0, 4);

        sendMessage[6] = (byte) GID;
        sendMessage[7] = (byte) 0;
        sendMessage[8] = requestID;

        int index = 9;
        for (String hostname : hostnames) {
            sendMessage[index] = (byte) '~';
            index++;

            System.arraycopy(hostname.getBytes("UTF-8"), 0, sendMessage, index, hostname.getBytes("UTF-8").length);

            index += hostname.getBytes("UTF-8").length;
        }

        short length = (short) index;
        System.arraycopy(shortToByte(length), 0, sendMessage, 4, 2);

        sendMessage[7] = calculateChecksum(sendMessage);


        DatagramPacket sendPacket = new DatagramPacket(sendMessage, sendMessage.length, serverAddress, portNum);

        packetSocket.send(sendPacket);
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

    private static byte calculateChecksum(byte[] message) {
        byte sum = (byte) 0;

        for (byte aMessage : message) {
            sum += aMessage;
        }

        return (byte) ~sum;
    }
}
