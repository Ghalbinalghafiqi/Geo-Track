<?php
include 'firebase.php';

date_default_timezone_set('Asia/Jakarta');
//------------------------------------------------------------------------------
// Curl HTTP Data dari API
function curl_get_contents($url)
{
    $ch = curl_init();
    curl_setopt($ch, CURLOPT_HEADER, 0);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch, CURLOPT_URL, $url);
    $data = curl_exec($ch);
    curl_close($ch);
    return $data;
}
//------------------------------------------------------------------------------
$json = json_encode([]);
//------------------------------------------------------------------------------
// Kirim Pesan Text
if (isset($_GET['token'], $_GET['chat_id'], $_GET['text'])) {
    $token = $_GET['token'];
    $chat_id = $_GET['chat_id'];

    if (isset($_GET['latitude'], $_GET['longitude'])) {
        $latitude = $_GET['latitude'];
        $longitude = $_GET['longitude'];
        $url = 'https://api.telegram.org/bot' . $token . '/sendLocation?chat_id=' . $chat_id . '&latitude=' . $latitude . '&longitude=' . $longitude;
        $request = curl_get_contents($url);
    }

    if (!empty($_GET['text'])) {
        $text = urlencode($_GET['text']);
        $url = 'https://api.telegram.org/bot' . $token . '/sendMessage?chat_id=' . $chat_id . '&text=' . $text . '&parse_mode=Markdown';
        if (isset($_GET['reply_markup'])) {
            $reply_markup = $_GET['reply_markup'];
            $url = 'https://api.telegram.org/bot' . $token . '/sendMessage?chat_id=' . $chat_id . '&text=' . $text . '&parse_mode=Markdown&reply_markup=' . urlencode($reply_markup);
        }
        $request = curl_get_contents($url);
    }

    $data = json_decode($request, true);
    $text = $data['result'][0]['text'];
    $query = [
        "ok" => $data['ok'],
        'datetime' => [
            'year' => date('Y'),
            'month' => date('m'),
            'day' => date('d'),
            'hour' => date('H'),
            'minute' => date('i'),
            'second' => date('s'),
        ],
    ];
    $json = json_encode($query);
}
//------------------------------------------------------------------------------
// Kirim Lokasi
else if (isset($_GET['token'], $_GET['chat_id'], $_GET['longitude'], $_GET['latitude'])) {
    $text = urlencode($_GET['text']);
    $token = $_GET['token'];
    $chat_id = $_GET['chat_id'];
    $latitude = $_GET['latitude'];
    $longitude = $_GET['longitude'];
    $url = 'https://api.telegram.org/bot' . $token . '/sendLocation?chat_id=' . $chat_id . '&latitude=' . $latitude . '&longitude=' . $longitude;
    $request = curl_get_contents($url);
    $data = json_decode($request, true);
    $text = $data['result'][0]['text'];
    $query = [
        "ok" => $data['ok'],
        'datetime' => [
            'year' => date('Y'),
            'month' => date('m'),
            'day' => date('d'),
            'hour' => date('H'),
            'minute' => date('i'),
            'second' => date('s'),
        ],
    ];
    $json = json_encode($query);
}
//------------------------------------------------------------------------------
else if (isset($_GET['token'], $_GET['limit'], $_GET['offset'])) {
    // Kirim Firebase
    if (isset($_GET['firebase'])) {
        $data = json_decode($_GET['firebase'], true);
        $response = sendDataToFirebase('/', $data);
        // echo $response;
        // die;
    }
    // Telegram Read
    $token = $_GET['token'];
    $limit = $_GET['limit'];
    $offset = $_GET['offset'];
    $url = "https://api.telegram.org/bot$token/getUpdates?limit=$limit&offset=$offset";
    $request = curl_get_contents($url);
    $data = json_decode($request, true);

    // Parse JSON Telegram API
    $chat_id = $data['result'][0]['message']['chat']['id'];
    $update_id = $data['result'][0]['update_id'];
    $text = $data['result'][0]['message']['text'];
    $latitude = $data['result'][0]['message']['location']['latitude'];
    $longitude = $data['result'][0]['message']['location']['longitude'];
    $button =  $data['result'][0]['callback_query']['data'];

    // Json Response
    $query = [
        'ok' => $data['ok'],
        'chat_id' => (!empty($chat_id)) ? $chat_id : 0,
        'update_id' => (!empty($update_id)) ? $update_id : 0,
        'text' => (!empty($text)) ? $text : "",
        'location' => [
            'latitude' => (!empty($latitude)) ? $latitude : 0,
            'longitude' => (!empty($longitude)) ? $longitude : 0,
        ],
        // 'button' => (!empty($button)) ? $button : "",
        'datetime' => [
            'year' => date('Y'),
            'month' => date('m'),
            'day' => date('d'),
            'hour' => date('H'),
            'minute' => date('i'),
            'second' => date('s'),
        ],
    ];
    $json = json_encode($query);
} else {
    $json = json_encode([]);
}
echo '$' . $json . '@';
//------------------------------------------------------------------------------