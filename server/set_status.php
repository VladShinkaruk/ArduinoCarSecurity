<?php
$state = $_GET['state'] ?? 'OFF';
file_put_contents('status.txt', $state);
echo "OK";
?>