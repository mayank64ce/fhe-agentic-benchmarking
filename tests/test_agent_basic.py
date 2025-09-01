from agents.react_agent import ReactAgent

model = "deepseek/deepseek-chat-v3.1:free"
agent = ReactAgent(tools=[], model=model)

output = agent.run("Write me a poem about a cat.")
print(output)