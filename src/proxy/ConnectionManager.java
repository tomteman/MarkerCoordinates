package proxy;

import java.io.IOException;
import java.io.OutputStream;
import java.net.URLConnection;

import org.apache.http.HttpResponse;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.DefaultHttpClient;
import org.json.JSONException;
import org.json.JSONObject;

public class ConnectionManager {
	String url = "http://www.google.com/";
	String charset = "UTF-8";
	URLConnection conn;
	public ConnectionManager()
	{
		
	}
	
    private void updatePawn(int id, float x, float y) {
    	try {
    	// Create a new HttpClient and Post Header
    	HttpClient client = new DefaultHttpClient();  
        HttpPost post = new HttpPost("http://www.google.com/");   
        post.setHeader("Content-type", "application/json");
        post.setHeader("Accept", "application/json");
        JSONObject obj = new JSONObject();
        obj.put("x", String.format("%.3f", x));
        obj.put("y", String.format("%.3f", y));
        post.setEntity(new StringEntity(obj.toString(), "UTF-8"));
        HttpResponse response = client.execute(post);  
    	
    	
    	

			
        } catch (IOException e) {
			// TODO Auto-generated catch block
        	e.printStackTrace();
		
        } catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
    	
   }

    void writeFloat(OutputStream out, float aFloat) throws IOException
    {
        int floatBits = Float.floatToIntBits(aFloat);
        
        writeInt(out, floatBits);
    }
    void writeInt(OutputStream out, int anInt) throws IOException
    {
        out.write(anInt & 0x000000FF);
        out.write((anInt & 0x0000FF00) >> 8);
        out.write((anInt & 0x00FF0000) >> 16);
        out.write((anInt & 0xFF000000) >> 24);
        out.flush();
    }
   public native void register();
	
}
