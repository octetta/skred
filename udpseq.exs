# udp_sender.exs
defmodule UdpSender do
  def start(host, port, message, interval_ms) do
    {:ok, socket} = :gen_udp.open(0, [:binary])  # 0 = any local port
    target = String.to_charlist(host)

    loop(socket, target, port, message, interval_ms)
  end

  defp loop(socket, host, port, message, interval_ms) do
    :ok = :gen_udp.send(socket, host, port, message)
    :timer.sleep(interval_ms)
    loop(socket, host, port, message, interval_ms)
  end
end

# --- run script ---
[host, port_str, interval_str, message] = System.argv()

port = String.to_integer(port_str)
interval = String.to_integer(interval_str)

UdpSender.start(host, port, message, interval)
