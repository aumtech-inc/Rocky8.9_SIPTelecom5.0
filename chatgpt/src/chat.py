import openai
import sys

openai.api_key = "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"


# ROLES: system, assistant, user
# system role:  system developer; it can provide instructions or context
# assistant role: chatGPT server
# user role: the person who is asking the question or chatting with chatGPT
#
completion = openai.ChatCompletion.create(
  model = "gpt-3.5-turbo",
  temperature = 0.8,
  max_tokens = 2000,
  messages = [
    {"role": "system", "content": "What is the intent of the user?"},
    {"role": "user", "content": "How may I help you?" },
    {"role": "user", "content": "I want to check the status of my application." }
  ]
)

print(completion.choices[0].message)
print(" ")
print(completion.choices[0].message.content)
print(completion)
