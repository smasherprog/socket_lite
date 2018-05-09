# socket_lite

<p>Windows <img src="https://ci.appveyor.com/api/projects/status/gupob5nj63onam9l?svg=true"/><p>
<p>Linx/Mac <img src="https://travis-ci.org/smasherprog/socket_lite.svg?branch=master"/><p>

<p>This library is a cross-platform networking library with a similiar API to ASIO.</p>

<h3>Platform Benchmarks vs ASIO as a baseline</h3>
<p>ST = Single Threaded  MT = Multi-threaded</p>
<p>Negative numbers mean ASIO is faster, where positive numbers mean Socket_Lite is faster</p>
<table>
 <thead>
   <tr>
     <th></th>
     <th>Windows Socket Lite</th> 
     <th>Linux ASIO</th>
   </tr>
  </thead>
  <tbody>
     <tr>
     <td>ST Connections Per Second</td>
      <td>625%:fire:</td>
     <td></td>  
    </tr>
  <tr>
     <td>ST Echos Per Second</td>
     <td>0% (Usually equal)</td>
     <td></td>  
    </tr>
   <tr>
     <td>MT Echos Per Second</td>
     <td>214%:fire:</td>
     <td></td>  
    </tr>
</tbody>
</table>
