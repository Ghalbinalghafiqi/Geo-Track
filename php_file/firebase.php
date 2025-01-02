<?php
// Firebase Database URL
$firebaseUrl = "https://belajarkontrolrelay-default-rtdb.asia-southeast1.firebasedatabase.app/";
// Firebase Secret or Server Key
$firebaseSecret = "AIzaSyAu9_HoUhX7HdmHZvagzSOow64UhiZDpgQ";
// Function to send data to Firebase
function sendDataToFirebase($path, $data)
{
    global $firebaseUrl, $firebaseSecret;

    $url = $firebaseUrl . $path . ".json?auth=" . $firebaseSecret;

    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, $url);
    curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_POST, 1);
    curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($data));
    curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: application/json'));

    $response = curl_exec($ch);
    curl_close($ch);

    return $response;
}
