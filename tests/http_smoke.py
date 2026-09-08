import requests
import os
import random
import string

# Server configuration
SERVER_HOST = 'localhost'  # Change to your server IP if needed
SERVER_PORT = 9906         # Change to your server port
BASE_URL = f'http://{SERVER_HOST}:{SERVER_PORT}'

def test_get_request():
    """Test basic GET requests"""
    print("\n=== Testing GET Requests ===")
    
    # Test root request (should redirect to judge.html)
    response = requests.get(BASE_URL)
    print(f"GET / -> Status: {response.status_code}, Length: {len(response.text)}")
    
    # Test existing files
    test_files = ['/judge.html', '/log.html', '/register.html', 
                  '/picture.html', '/video.html', '/fans.html']
    
    for file in test_files:
        response = requests.get(BASE_URL + file)
        print(f"GET {file} -> Status: {response.status_code}, Length: {len(response.text)}")
    
    # Test non-existent file
    response = requests.get(BASE_URL + '/nonexistent.html')
    print(f"GET /nonexistent.html -> Status: {response.status_code}")

def test_post_requests():
    """Test POST requests for login and registration"""
    print("\n=== Testing POST Requests ===")
    
    # Generate random user credentials for testing
    username = ''.join(random.choices(string.ascii_letters, k=8))
    password = ''.join(random.choices(string.ascii_letters + string.digits, k=12))
    
    # Test registration (POST to /2)
    print("\nTesting registration:")
    data = f"user={username}&passwd={password}"
    headers = {'Content-Type': 'application/x-www-form-urlencoded'}
    
    response = requests.post(BASE_URL + '/2', data=data, headers=headers)
    print(f"POST /2 (register) -> Status: {response.status_code}, Length: {len(response.text)}")
    print(f"Response URL: {response.url}")
    
    # Test login with correct credentials (POST to /3)
    print("\nTesting login with correct credentials:")
    response = requests.post(BASE_URL + '/3', data=data, headers=headers)
    print(f"POST /3 (login) -> Status: {response.status_code}, Length: {len(response.text)}")
    print(f"Response URL: {response.url}")
    
    # Test login with incorrect credentials
    print("\nTesting login with incorrect credentials:")
    wrong_data = f"user={username}&passwd=wrongpassword"
    response = requests.post(BASE_URL + '/3', data=wrong_data, headers=headers)
    print(f"POST /3 (login) -> Status: {response.status_code}, Length: {len(response.text)}")
    print(f"Response URL: {response.url}")

def test_error_handling():
    """Test server error handling"""
    print("\n=== Testing Error Handling ===")
    
    # Test malformed request
    print("\nTesting malformed request:")
    try:
        response = requests.get(BASE_URL + '/invalid request')
        print(f"Malformed request -> Status: {response.status_code}")
    except requests.exceptions.RequestException as e:
        print(f"Malformed request failed as expected: {str(e)}")
    
    # Test non-existent path
    print("\nTesting non-existent path:")
    response = requests.get(BASE_URL + '/nonexistent/path')
    print(f"Non-existent path -> Status: {response.status_code}")

def test_keep_alive():
    """Test keep-alive connections"""
    print("\n=== Testing Keep-Alive ===")
    
    session = requests.Session()
    
    # First request
    response = session.get(BASE_URL, headers={'Connection': 'keep-alive'})
    print(f"First request -> Status: {response.status_code}, Connection: {response.headers.get('Connection')}")
    
    # Second request using same connection
    response = session.get(BASE_URL + '/judge.html')
    print(f"Second request -> Status: {response.status_code}, Connection: {response.headers.get('Connection')}")
    
    session.close()

def run_all_tests():
    """Run all test cases"""
    test_get_request()
    test_post_requests()
    test_error_handling()
    test_keep_alive()

if __name__ == '__main__':
    print("Starting HTTP Server Tests...")
    run_all_tests()
    print("\nAll tests completed!")