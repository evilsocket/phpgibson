<?php

$g = new Gibson();
var_dump( $g->connect( "unix:///home/evilsocket/Desktop/Dev/gibson/gibson.sock" ) );
var_dump( $g->stats() );
var_dump( $g->set( "foo", "bar" ) );
var_dump( $g->get( "foo" ) );
var_dump( $g->mget( "f" ) );
var_dump( $g->get( "lol" ) );
var_dump( $g->getLastError() );

?>
